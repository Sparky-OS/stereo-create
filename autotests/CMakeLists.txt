# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
include(ECMAddTests)
ecm_add_tests(enginetest.cpp LINK_LIBRARIES stereocreate_engine Qt6::Test)
