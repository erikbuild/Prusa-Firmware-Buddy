#!/bin/bash

if [[ "$#" -ne 2 ]]; then
    echo "Usage: $0 <path_to_compile_commands_json> <source_file_name>"
    echo -e "\tsource_file_name - matched with grep, should be unique, if more match all include path will be provided"
    echo -e "\nREQUIREMENTS\n\trequires jq to parse the compile_commands.json"
    exit 1
fi

jq ".[].command" "$1" | grep "$2" | tr -d "\"" | tr " " "\n" | grep "^-I" | sed -E "s@-I$PWD/@@g" | sort -u
