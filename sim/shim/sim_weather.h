#pragma once

// Canned weather conditions, cycled from the TUI so every icon branch in
// WeatherPlugin can be seen without waiting on real weather.
void simSetWeather(int tempC, int weatherCode);
int simWeatherTempC();
int simWeatherCode();
int simWeatherCount();
void simCycleWeather();
const char *simWeatherLabel();
