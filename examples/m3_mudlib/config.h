#ifndef M3_CONFIG_H
#define M3_CONFIG_H

// We call LLM through a local LiteLLM Proxy that translates OpenAI-compatible
// REST API calls into LLM-specific API calls.
//
// Requires: PACKAGE_CURL=ON, PACKAGE_JSON=ON (default in dev builds).

#define LLM_API_KEY         "/.master_key"
#define LLM_RESTAPI_MODELS  "http://localhost:4010/models"

#endif // !M3_CONFIG_H
