#ifndef M3_CONFIG_H
#define M3_CONFIG_H

/*
 * The example code is intended to be model-agnostic and calls LLM on a local
 * proxy server. This enables testing with any LLM supported by the proxy,
 * without needing to change the example code.
 *
 * For example, to run with the LiteLLM Proxy Server using Gemini Flash model,
 * start the proxy with:
 *
 *   litellm --model gemini/gemini-2.0-flash --port 4010 
 *
 * M3 uses OpenAI-compatible endpoints supported by the LiteLLM Proxy, which
 * are documented here:
 *   https://docs.litellm.ai/docs/supported_endpoints
 */
#define LLM_E_MODELS  "http://localhost:4010/models"

#endif // !M3_CONFIG_H
