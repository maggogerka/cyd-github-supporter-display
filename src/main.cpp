#include <Arduino.h>

#include "app/application.h"

Application application;

void setup() { application.begin(); }

void loop() { application.update(); }
