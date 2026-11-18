#pragma once

#include <QString>

/** Sends libobs and app messages to a dated file in the logs folder, keeping the ten newest. */
void setupLogging();
QString currentLogFile();
