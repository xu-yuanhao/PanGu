#!/usr/bin/env node
// T09 Pi-side oracle runner. Development tool only; never part of the native product.
//
// Usage:
//   node oracle_pi.mjs --oracle <pi-work-copy> --scenarios <t09-scenarios.json> --output <new-dir>
//
// Writes one raw JSONL trace per case plus a run.json summary. The model is
// scripted; no network, credentials or real provider is used. The oracle copy
// must be a separate writable work copy of the frozen reference, never
// references/ itself.

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { createHash } from "node:crypto";
import { fileURLToPath } from "node:url";
const digest = (path) => createHash("sha256").update(readFileSync(path)).digest("hex");

function parseArgs(argv) {
  const args = {};
  for (let i = 0; i < argv.length; i += 2) {
    const key = argv[i];
    if (!key.startsWith("--")) throw new Error("unexpected argument " + key);
    args[key.slice(2)] = argv[i + 1];
  }
  return args;
}

const args = parseArgs(process.argv.slice(2));
for (const required of ["oracle", "scenarios", "output"]) {
  if (!args[required]) throw new Error("missing --" + required);
}
const oracle = resolve(args.oracle);
const output = resolve(args.output);
const scenarios = JSON.parse(readFileSync(args.scenarios, "utf8"));

const { runAgentLoop } = await import(join(oracle, "packages/agent/src/agent-loop.ts"));
const { createAssistantMessageEventStream } = await import(join(oracle, "packages/ai/src/index.ts"));
const { Type } = await import(join(oracle, "node_modules/typebox/build/index.mjs"));

mkdirSync(output, { recursive: false });

const usage = {
  input: 0,
  output: 0,
  cacheRead: 0,
  cacheWrite: 0,
  totalTokens: 0,
  cost: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0, total: 0 },
};

const model = {
  id: "scripted",
  name: "scripted",
  api: "openai-responses",
  provider: "scripted",
  baseUrl: "https://example.invalid",
  reasoning: false,
  input: ["text"],
  cost: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0 },
  contextWindow: 8192,
  maxTokens: 2048,
};

// Turn a scripted block list into the Pi stream event sequence. The script is
// data only: it cannot run code, touch the network or choose a provider.
function scriptedStream(blocks) {
  return (signal) => {
    const stream = createAssistantMessageEventStream();
    queueMicrotask(() => {
      if (signal?.aborted) {
        stream.push({
          type: "error",
          reason: "aborted",
          error: {
            role: "assistant",
            content: [],
            api: model.api,
            provider: model.provider,
            model: model.id,
            usage,
            stopReason: "aborted",
            errorMessage: "Operation aborted",
            timestamp: 0,
          },
        });
        return;
      }
      const partial = {
        role: "assistant",
        content: [],
        api: model.api,
        provider: model.provider,
        model: model.id,
        usage,
        stopReason: "stop",
        timestamp: 0,
      };
      stream.push({ type: "start", partial });
      let content = [];
      for (const block of blocks) {
        if (block.type === "text") {
          content = [...content, { type: "text", text: block.text }];
          stream.push({ type: "text_start", contentIndex: content.length - 1, partial: { ...partial, content } });
          stream.push({
            type: "text_delta",
            contentIndex: content.length - 1,
            delta: block.text,
            partial: { ...partial, content },
          });
          stream.push({ type: "text_end", contentIndex: content.length - 1, partial: { ...partial, content } });
        } else if (block.type === "toolCall") {
          content = [
            ...content,
            { type: "toolCall", id: block.id, name: block.name, arguments: block.arguments },
          ];
          stream.push({
            type: "toolcall_start",
            contentIndex: content.length - 1,
            partial: { ...partial, content },
          });
          stream.push({
            type: "toolcall_end",
            contentIndex: content.length - 1,
            toolCall: content[content.length - 1],
            partial: { ...partial, content },
          });
        }
      }
      const final = { ...partial, content };
      stream.push({ type: "done", reason: "stop", message: final });
    });
    return stream;
  };
}

function makeTools(script) {
  return script.tools.map((tool) => ({
    name: tool.name,
    label: tool.name,
    description: tool.description ?? "",
    parameters: Type.Object({}),
    execute: async (toolCallId, params) => {
      if (tool.fail) {
        return { content: [{ type: "text", text: tool.fail }], details: {}, isError: true };
      }
      return { content: [{ type: "text", text: tool.result ?? "" }], details: {} };
    },
  }));
}

function makeConfig(caseDef) {
  let turn = 0;
  const streams = caseDef.turns.map((turnDef) => scriptedStream(turnDef.blocks));
  return {
    model,
    convertToLlm: (messages) =>
      messages.filter((m) => ["system", "user", "assistant", "toolResult"].includes(m.role)),
    toolExecution: caseDef.tool_execution ?? "parallel",
    streamFn: (_model, _context, options) => {
      const factory = streams[Math.min(turn, streams.length - 1)];
      turn += 1;
      return factory(options?.signal);
    },
    ...(caseDef.steering
      ? {
          getSteeringMessages: (() => {
            const queued = [...caseDef.steering];
            return async () => (queued.length ? [queued.shift()] : []);
          })(),
        }
      : {}),
  };
}

function userMessage(text) {
  return { role: "user", content: text, timestamp: 0 };
}

const runSummary = {
  schema: "pangu.t09.oracle-run.v1",
  reference_commit: scenarios.reference_commit,
  manifest_sha256: digest(args.scenarios),
  runner_sha256: digest(fileURLToPath(import.meta.url)),
  runtime: process.version,
  source_artifacts: Object.fromEntries(["package-lock.json", "packages/agent/src/agent-loop.ts", "packages/agent/src/types.ts", "packages/ai/src/index.ts"].map((path) => [path, digest(join(oracle, path))])),
  provenance_scope: "selected entry files and lockfile; full installed closure not attested",

  scenario_manifest: args.scenarios,
  cases: [],
  status: "BLOCKED",
  candidate_native: "NOT_RUN",
};

for (const caseDef of scenarios.cases) {
  const rawPath = join(output, caseDef.case_id + ".raw.jsonl");
  const events = [];
  const emit = (event) => {
    events.push(event);
  };
  // The prompt is added by runAgentLoop itself; the context holds only prior history.
  const context = {
    messages: [],
    tools: makeTools(caseDef),
  };
  const config = makeConfig(caseDef);
  const controller = new AbortController();
  let exit = 0;
  let error = null;
  try {
    // Signature order: prompts, context, config, emit, signal, streamFn.
    await runAgentLoop(
      [userMessage(caseDef.prompt)],
      context,
      config,
      (event) => {
        emit(event);
        if (caseDef.cancel_after_events !== undefined && events.length >= caseDef.cancel_after_events) {
          controller.abort();
        }
      },
      controller.signal,
      config.streamFn,
    );
  } catch (reason) {
    exit = 1;
    error = String(reason);
  }
  const lines = events.map((event, index) => ({
    seq: index,
    event: structuredClone(event),
  }));
  writeFileSync(rawPath, lines.map((line) => JSON.stringify(line)).join("\n") + "\n");
  runSummary.cases.push({
    case_id: caseDef.case_id,
    feature_ids: caseDef.feature_ids ?? [],
    scenario_revision: caseDef.scenario_revision ?? 1,
    exit,
    error,
    event_count: lines.length,
    raw_artifact: rawPath,
    raw_sha256: digest(rawPath),
  });
}

if (runSummary.cases.some((item) => item.exit !== 0 || item.event_count === 0)) process.exitCode = 1;
writeFileSync(join(output, "run.json"), JSON.stringify(runSummary, null, 2) + "\n");
console.log(JSON.stringify({ status: "BLOCKED", cases: runSummary.cases.length, output }));

