// example MUD application: Hello OpenAI
//
// Sends "hello" to the OpenAI Chat Completions API and prints the reply.
//
// Usage:
//   neolith -c hello_openai.c
//
// The API key is read from a file named ".openai_api_key" in the same
// directory as this script.  Create it with:
//   echo "sk-..." > examples/apps/.openai_api_key
//
// Requires: PACKAGE_CURL=ON, PACKAGE_JSON=ON (default in dev builds).

#define OPENAI_URL    "https://api.openai.com/v1/chat/completions"
#define OPENAI_MODEL  "gpt-4o"
#define KEY_FILE      ".openai_api_key"

string api_key;

void send_hello();
void on_response(int success, mixed body_or_error);

// -----------------------------------------------------------------------
// MUD application entry points
// -----------------------------------------------------------------------

object connect(int port) {
    return this_object();   // single-user console mode
}

void logon() {
    string raw_key;

    // Read the API key from a local file (trim trailing whitespace).
    raw_key = read_file(KEY_FILE, 0, 1);
    if (!raw_key || raw_key == "") {
        write("Error: API key file '" + KEY_FILE + "' not found or empty.\n");
        write("Create it with: echo 'sk-...' > examples/apps/.openai_api_key\n");
        shutdown();
        return;
    }

    // Trim trailing newline / spaces.
    while (raw_key != "" && (raw_key[<1] == '\n' || raw_key[<1] == '\r' || raw_key[<1] == '\t' || raw_key[<1] == ' '))
        raw_key = raw_key[0..<2];

    api_key = raw_key;

    write("Connecting to OpenAI...\n");
    send_hello();
}

// -----------------------------------------------------------------------
// Request
// -----------------------------------------------------------------------

void send_hello() {
    mapping request_body;
    string auth_header;

    // Build the request payload as an LPC mapping, then serialize to JSON.
    request_body = ([
        "model"    : OPENAI_MODEL,
        "messages" : ({ ([ "role" : "user", "content" : "hello" ]) })
    ]);

    auth_header = "Authorization: Bearer " + api_key;

    // Configure the CURL handle for this object.
    perform_using("url",      OPENAI_URL);
    perform_using("headers",  ({ "Content-Type: application/json", auth_header }));
    perform_using("body",     request_body.to_json());
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
