// example MUD application: Hello World

object connect(int port) {
    return this_object(); // single-user mode
}

void logon() {
    write ("Hello World from LPC!\n");
    shutdown();
}
