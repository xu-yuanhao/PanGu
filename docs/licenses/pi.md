# Pi 上游许可

来源：earendil-works/pi，提交 d5b3de22d0e24a3e9c38a9410d3329b099717283。

本批移植来源：packages/chord/src/delta/pending.ts、packages/durable/src/session/transaction.ts 的 storeHeld/holdable/adoptHeld、session/session.ts 的提交选择；原生实现为 src/runtime/pending.S、document_tx.S、delta.S、records.S。

Durable 上下文与压缩移植来源：packages/durable/src/harness/{context,compaction}.ts、packages/ai/src/utils/{estimate,text,transcript}.ts；对应原生实现为 durable_generation.S 和 durable_compaction.S，包含摘要提示词。

MIT License

Copyright (c) 2025 Mario Zechner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
