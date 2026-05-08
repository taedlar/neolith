// example MUD application: List models available
//
// Usage:
//   neolith -f m3.conf -c m3_mudlib/agent/models.c
//

string master_key;

void send_query();
void on_response(int success, mixed body_or_error);

// -----------------------------------------------------------------------
// MUD application entry points
// -----------------------------------------------------------------------

object connect(int port) {
    return this_object();   // single-user console mode
}

void logon() {
    // Read the LiteLLM master key from a local file (trim trailing whitespace).
    master_key = read_file(LLM_API_KEY, 0, 1);
    if (!master_key || master_key == "") {
        write("Error: LiteLLM master key file '" + LLM_API_KEY + "' not found or empty.\n");
        write("Create it with: echo 'sk-...' > examples/m3_mudlib/.master_key\n");
        shutdown();
        return;
    }

    // LiteLLM master key must start with sk-. Trim trailing newline / spaces.
    while (master_key != "" && (master_key[<1] == '\n' || master_key[<1] == '\r' || master_key[<1] == '\t' || master_key[<1] == ' '))
        master_key = master_key[0..<2];

    send_query();
}

// -----------------------------------------------------------------------
// Request
// -----------------------------------------------------------------------

void send_query() {
    // Configure the CURL handle for this object.
    perform_using("url",      LLM_RESTAPI_MODELS);
    perform_using("headers",  ({ "Authorization: Bearer " + master_key }));
    perform_using("timeout_ms", 30000);

    // Submit the non-blocking HTTP POST; on_response() is called on completion.
    perform_to("on_response", 0);
}

// -----------------------------------------------------------------------
// Response callback — signature: (int success, buffer|string body_or_error)
// -----------------------------------------------------------------------

void on_response(int success, mixed body_or_error) {
    mixed parsed, data, item;

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

    data = parsed["data"];
    if (!arrayp(data) || sizeof(data) == 0) {
        write("No data in response.\n");
        write("Full response: " + parsed.to_json() + "\n");
        shutdown();
        return;
    }

    write("Available models:\n");
    foreach (item in data) {
        if (mapp(item) && stringp(item["id"]))
            write("  " + item["id"] + "\n");
    }
    shutdown();
}
