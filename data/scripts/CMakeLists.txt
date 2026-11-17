# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

install(DIRECTORY osmo360 DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts
        FILES_MATCHING PATTERN "*.py" PATTERN "LICENSE-PanoForge" PATTERN "README.md"
        PATTERN "__pycache__" EXCLUDE)

add_subdirectory(automask)
add_subdirectory(vosk)
add_subdirectory(whisper)

INSTALL(FILES 
    checkpackages.py
    otiointerface.py
    checkgpu.py
    generate_transition_previews.py

DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/scripts)
