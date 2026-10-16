#pragma once
#include "connection_setup_policy.h"
void connectionSetupBegin(bool resetHeld);
bool connectionSetupActive();
SetupPage connectionSetupPage();
const char* connectionSetupError();
void connectionSetupTick(bool a,bool b,bool aClicked,bool bClicked);
