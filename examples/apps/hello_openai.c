// example MUD application: Hello OpenAI
//
// Sends "hello" to the OpenAI Chat Completions API and prints the reply.
//
// Usage:
//   OPENAI_API_KEY=sk-... neolith -c hello_openai.c
//
// Optional environment variables (shown with their defaults):
//   OPENAI_BASE_URL  https://api.openai.com/v1
//   OPENAI_MODEL     gpt-4o
//   OPENAI_API_KEY   (required — no default)
//
// Requires: PACKAGE_CURL=ON, PACKAGE_JSON=ON (default in dev builds).

void send_hello();
void on_response(int success, mixed body_or_error);

// -----------------------------------------------------------------------
// MUD application entry points
// -----------------------------------------------------------------------

object connect(int port) {
    return this_object();   // single-user console mode
}

void logon() {
    string api_key;

    api_key = envsubst("${OPENAI_API_KEY}");
    if (!api_key || api_key == "") {
        write("Error: OPENAI_API_KEY environment variable is not set.\n");
        shutdown();
        return;
    }

    write("Connecting to OpenAI...\n");
    send_hello();
}

// -----------------------------------------------------------------------
// Request
// -----------------------------------------------------------------------

void send_hello() {
    mapping request_body;

    // Build the request payload as an LPC mapping, then serialize to JSON.
    request_body = ([
        "model"    : envsubst("${OPENAI_MODEL:-gpt-4o}"),
        "messages" : ({ ([ "role" : "user", "content" : "hello" ]) })
    ]);

    // Configure the CURL handle for this object.
    perform_using("url",     envsubst("${OPENAI_BASE_URL:-https://api.openai.com/v1}") + "/chat/completions");
    perform_using("headers", ({
        "Content-Type: application/json",
        envsubst("Authorization: Bearer ${OPENAI_API_KEY}")
    }));
    perform_using("body",       request_body.to_json());
    perform_using("timeout_ms", 30000);

    // Submit the non-blocking HTTP POST; on_response() is called on completion.
    perform_to("on_response", 0);
}

// -----------------------------------------------------------------------
// Response callback — signature: (int success, buffer|string body_or_error)
// -----------------------------------------------------------------------

void on_response(int success, mixed body_or_error) {
    mixed parsed;
    mixed choices;
    string reply_text;

    if (!success) {
        write("Request failed: " + body_or_error + "\n");
        shutdown();
        return;
    }

    // body_or_error is a buffer on success; convert to LPC mapping.
    parsed = from_json(body_or_error);
    if (!mapp(parsed)) {
        write("Unexpected response format.\n");
        shutdown();
        return;
    }

    choices = parsed["choices"];
    if (!arrayp(choices) || sizeof(choices) == 0) {
        write("No choices in response.\n");
        write("Full response: " + parsed.to_json() + "\n");
        shutdown();
        return;
    }

    reply_text = choices[0]["message"]["content"];

    write("OpenAI says: " + reply_text + "\n");
    shutdown();
}
