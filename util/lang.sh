#!/bin/bash
#
# Attract-Mode Plus language utility script
# - Updates *.msg files to match the master_msg file (en.msg)
# - Additional translations will be placed at the end of their respective file
# - All comments in the translation files will be lost
# - Run from the attractplus folder using:
#
#   ./util/lang.sh

language_path=resources/language
master_msg=$language_path/en.msg

# Ensure path exists
if [ ! -d "$language_path" ]; then
    echo "Error, $language_path subdirectory not found"
    exit 1
fi

# Ensure master message exists
if [ ! -f "$master_msg" ]; then
    echo "Error, $master_msg not found"
    exit 1
fi

# Read all the master translations
# - These will be used to check for "extra" (unused) language translations
master_key=""
declare -A master
while IFS=";" read -r key value; do
    if [[ -z "$master_key" && "${key:0:2}" == "#@" ]]; then
        master_key=$key
        continue
    fi
    [[ -z "$key" || "${key:0:1}" == "#" ]] && continue
    master["$key"]="$value"
done <<< $(tr -d '\r' < "$master_msg")


echo "Updating languages"
for f in $language_path/*.msg
do
    [[ "$f" == "$master_msg" ]] && continue
    echo "- $f"

    declare -A language
    declare -A missing_value
    declare -A missing_help

    # Read translations from the target file
    language_key=""
    while IFS=";" read -r key value; do
        if [[ -z "$language_key" && "${key:0:2}" == "#@" ]]; then
            language_key=$key
        fi
        [[ -z "$key" || "${key:0:1}" == "#" ]] && continue
        language["$key"]="$value"
    done <<< $(tr -d '\r' < "$f")

    # Write the master structure using the given translations
    echo -n "" > "$f"
    while IFS=";" read -r key value; do
        if [[ "$key" == "$master_key" ]]; then
            # The #@Language header at the top of the file
            echo "$language_key" >> "$f"
        elif [[ -z "$key" || "${key:0:1}" == "#" ]]; then
            # Master comments left as-is
            echo "$key" >> "$f"
        else
            if [[ -v language['$key'] ]]; then
                if [[ "$value" != "${language[$key]}" ]]; then
                    # Transition is same as English
                    echo "$key;${language[$key]}" >> "$f"
                else
                    # Translation found
                    echo "$key;${language[$key]}" >> "$f"
                fi
            else
                # Missing translation commented out
                echo "#$key;" >> "$f"
                if [[ "${key:0:1}" == "_" ]]; then
                    missing_help["$key"]=""
                else
                    missing_value["$key"]=""
                fi
            fi
        fi
    done <<< $(tr -d '\r' < "$master_msg")

    # Write extra (unused) translations to the end
    for key in "${!language[@]}"; do
        if [[ ! -v master['$key'] ]]; then
            echo "$key;${language[$key]}" >> "$f"
        fi
    done

    # Write missing help to the end (duplicating them)
    if [ ${#missing_help[@]} -gt 0 ]; then
        echo "" >> "$f"
        echo "# -- missing_help --" >> "$f"
        for key in "${!missing_help[@]}"; do
            echo "#$key;${master[$key]}" >> "$f"
        done
    fi

    # Write missing value to the end (duplicating them)
    if [ ${#missing_value[@]} -gt 0 ]; then
        echo "" >> "$f"
        echo "# -- missing_value --" >> "$f"
        for key in "${!missing_value[@]}"; do
            echo "#$key;" >> "$f"
        done
    fi

    unset language
    unset missing_value
    unset missing_help
done
