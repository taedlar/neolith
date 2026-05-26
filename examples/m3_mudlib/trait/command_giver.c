/* trait/command_giver.c */

mapping aliases;

void create_command_giver_trait () {
  aliases = ([]);
}

void set_alias (string name, string command) {
  if (!aliases)
    aliases = ([]);
  aliases[name] = command;
}

void remove_alias (string name) {
  if (aliases)
    map_delete(aliases, name);
}

mixed query_alias (string name) {
  if (aliases && aliases[name])
    return aliases[name];
  return 0;
}

mapping query_aliases () {
  if (!aliases)
    aliases = ([]);
  return aliases;
}

string expand_alias (string input) {
  string verb;
  string rest;

  if (!input || input == "")
    return input;

  verb = query_verb();
  if (!verb || verb == "")
    {
      /* Fallback for contexts where query_verb() is unavailable. */
      if (sscanf(input, "%s %s", verb, rest) != 2)
        verb = input;
    }

  if (!aliases || !aliases[verb])
    return input;

  return replace_string(input, verb, aliases[verb], 1);
}

string process_input (string input) {
  return expand_alias(input);
}
