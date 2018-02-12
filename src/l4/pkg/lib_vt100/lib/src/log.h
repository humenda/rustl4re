#pragma once

#include <stdio.h>

#define LOGd(l, a...) do { if (l) printf(a); } while (0)

