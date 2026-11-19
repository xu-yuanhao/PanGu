#include <new>
#include <pangu/services/schema.h>
#include <string>
#include <valijson/adapters/picojson_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>
static bool parse_complete(picojson::value &value, const std::string &text) {
  std::string error;
  auto end = picojson::parse(value, text.begin(), text.end(), &error);
  if (!error.empty())
    return false;
  while (end != text.end() &&
         (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
    ++end;
  return end == text.end();
}
extern "C" pangu_status pg_schema_validate(void *, pangu_slice schema,
                                           pangu_slice args) {
  if ((!schema.data && schema.len) || (!args.data && args.len))
    return PANGU_INVALID;
  pangu_status status=pg_schema_check(schema,args,nullptr,nullptr);
  return status==PANGU_INVALID ? PANGU_TOOL_ERROR : status;
}

extern "C" pangu_status pg_schema_check(pangu_slice schema, pangu_slice args,
    pangu_arguments_sink diagnostic, void *ctx) {
  if ((!schema.data && schema.len) || (!args.data && args.len)) return PANGU_INVALID;
  try {
    picojson::value s, a;
    std::string detail;
    bool schema_error=false;
    if (!parse_complete(s, std::string(schema.data ? schema.data : "", schema.len)))
      { detail = "  - root: Invalid JSON Schema"; schema_error=true; }
    else if (!parse_complete(a, std::string(args.data ? args.data : "", args.len)))
      detail = "  - root: Invalid arguments JSON";
    else {
      valijson::Schema compiled;
      valijson::SchemaParser parser(valijson::SchemaParser::kDraft7);
      parser.populateSchema(valijson::adapters::PicoJsonAdapter(s), compiled);
      valijson::Validator validator(valijson::Validator::kStrongTypes);
      valijson::ValidationResults errors;
      if (validator.validate(compiled, valijson::adapters::PicoJsonAdapter(a), &errors))
        return PANGU_OK;
      for (const auto &error : errors) {
        std::string path = error.jsonPointer;
        if (!path.empty()) path.erase(0, 1);
        for (char &c : path) if (c == '/') c = '.';
        const std::string required = "Missing required property '";
        if (error.description.compare(0, required.size(), required)==0) {
          auto end=error.description.rfind("'.");
          if (end!=std::string::npos) {
            if (!path.empty()) path += '.';
            path += error.description.substr(required.size(), end-required.size());
          }
        }
        if (path.empty()) path = "root";
        if (!detail.empty()) detail += '\n';
        detail += "  - " + path + ": " + error.description;
      }
    }
    if (diagnostic) {
      pangu_status st = diagnostic(ctx, {detail.data(), detail.size()});
      if (st != PANGU_OK) return st;
    }
    return schema_error ? PANGU_INVALID : PANGU_TOOL_ERROR;
  } catch (const std::bad_alloc &) { return PANGU_NOMEM; }
    catch (...) {
      static const char message[]="  - root: Invalid JSON Schema (unresolvable reference or invalid keyword)";
      if (diagnostic) {
        pangu_status st=diagnostic(ctx,{message,sizeof(message)-1});
        if (st!=PANGU_OK) return st;
      }
      return PANGU_INVALID;
    }
}
