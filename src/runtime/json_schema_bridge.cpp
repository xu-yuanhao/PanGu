#include "json_schema_bridge.h"
#include <json-c/json.h>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
namespace {
struct Contract {nlohmann::json source;valijson::Schema schema;valijson::Validator validator;};
void error_text(char **out,const char *text) noexcept {
 if(!out)return;
 const auto size=std::strlen(text);auto copy=static_cast<char *>(std::malloc(size+1));
 if(copy)std::memcpy(copy,text,size+1);
 *out=copy;
}
}
extern "C" int pg_json_schema_compile(const char *text,void **out,char **error){
 if(error)*error=nullptr;
 if(!out)return 4;
 *out=nullptr;if(!text)return 4;
 try {
  auto contract=std::make_unique<Contract>();contract->source=nlohmann::json::parse(text);
  valijson::adapters::NlohmannJsonAdapter adapter(contract->source);
  valijson::SchemaParser parser;parser.populateSchema(adapter,contract->schema);
  *out=contract.release();return 0;
 }catch(const std::bad_alloc &){return 5;}
 catch(const std::exception &failure){error_text(error,failure.what());return 4;}
}
extern "C" int pg_json_schema_validate(void *opaque,const json_object *value,char **error){
 if(error)*error=nullptr;
 if(!opaque)return 4;
 try {
  auto &contract=*static_cast<Contract *>(opaque);
  const char *text=value?json_object_to_json_string_ext(const_cast<json_object *>(value),JSON_C_TO_STRING_PLAIN):"null";
  if(!text)return 5;
  auto target=nlohmann::json::parse(text);valijson::adapters::NlohmannJsonAdapter adapter(target);
  valijson::ValidationResults results;
  if(contract.validator.validate(contract.schema,adapter,&results))return 0;
  std::string path="root",description="invalid";
  if(results.begin()!=results.end()){
   path=results.begin()->jsonPointer;description=results.begin()->description;
   if(!path.empty()&&path.front()=='#')path.erase(0,1);
   if(!path.empty()&&path.front()=='/')path.erase(0,1);
   for(auto &character:path)if(character=='/')character='.';
   if(path.empty())path="root";
  }
  error_text(error,(path+": "+description).c_str());return 1;
 }catch(const std::bad_alloc &){return 5;}
 catch(const std::exception &failure){error_text(error,failure.what());return 1;}
}
extern "C" void pg_json_schema_close(void *opaque){delete static_cast<Contract *>(opaque);}
