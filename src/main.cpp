#include "./main.hpp"

void setup()
{
  M5.begin();
  static SIDExplorer *Explorer = new SIDExplorer();
}


void loop()
{
  vTaskDelete( NULL );
}

