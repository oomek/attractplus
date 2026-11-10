# Copy SDK sources without copying the submodule's Git metadata.
file(COPY "${SOURCE}/" DESTINATION "${DEST}" PATTERN ".git" EXCLUDE)
