// example MUD application: List models available
//
// Usage:
//   neolith -f m3.conf -c m3_mudlib/agent/models.c
//

void send_query();
void on_response(int success, mixed body_or_error);

// -----------------------------------------------------------------------
// MUD application entry points
// -----------------------------------------------------------------------

object connect(int port) {
    return this_object();   // single-user console mode
}

void logon() {
    send_query();
}

// -----------------------------------------------------------------------
// Request
// -----------------------------------------------------------------------

void send_query() {
    // Configure the CURL handle for this object.
    perform_using("url",      LLM_E_MODELS);
    /* perform_using("headers",  ({ "Authorization: Bearer " + master_key })); */
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
