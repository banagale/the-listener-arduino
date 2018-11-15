#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// max BUTTON_CT is 255 (based on NO_BUTTON)
#define BUTTON_CT 4
#define LEDS_PER_SEGMENT 25

#define UNCONNECTED_ANALOG_PIN 0
#define STRAND_PIN 9

Adafruit_NeoPixel strip = Adafruit_NeoPixel(BUTTON_CT * LEDS_PER_SEGMENT, STRAND_PIN, NEO_RGB + NEO_KHZ800);

#define RED strip.Color(255, 0, 0)
#define RED_DIM strip.Color(16, 0, 0)
#define GREEN strip.Color(0, 255, 0)
#define GREEN_DIM strip.Color(0, 16, 0)
#define BLUE strip.Color(0, 0, 255)
#define BLUE_DIM strip.Color(0, 0, 16)
#define PURPTY_DURP strip.Color(255, 0, 255)
#define PURPTY_DURP_DIM strip.Color(16, 0, 16)

#define PAUSE_BEFORE_LIGHT_MS 250
#define SHOW_LIGHT_MS 500
#define PAUSE_AFTER_LIGHT_MS 250
#define PATTERN_TERMINATOR 255
#define NO_BUTTON 255
#define MAX_PATTERN_LENGTH 255

typedef struct {
  uint8_t btn_pin;
  uint8_t led_nums[LEDS_PER_SEGMENT];
  const char* name;  // used for debugging only
  uint32_t dim_color;
  uint32_t color;
} btn;

enum mode {
  showing_pattern,
  awaiting_input,
};

typedef struct {
  unsigned long current_loop_start_ms;  // `millis()` at the time the current loop started
  bool current_loop_did_update_state;  // true if the state was changed at any time in the current loop
  
  uint8_t pattern[MAX_PATTERN_LENGTH + 1];  // array of ints for the pattern; PATTERN_TERMINATOR used to terminate pattern
  uint8_t active_button;  // first button pressed for this step in the pattern
  uint8_t lifted_button;  // the active_button that was just un-pressed
  bool buttons_pressed_state[BUTTON_CT];  // array of whether each button is currently pressed or not
  mode current_mode;  // showing the pattern or awaiting player input
  uint8_t current_step;  // what step in the pattern we're currently working with
  unsigned long current_step_since_ms;  // when current_step was last updated
} game_state;

const btn BUTTONS[] = {
  (btn) {2, {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24}, "BROWN", BLUE_DIM, BLUE},
  (btn) {3, {25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49}, "GREEN", GREEN_DIM, GREEN},
  (btn) {4, {50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74}, "YELLOW", PURPTY_DURP_DIM, PURPTY_DURP},
  (btn) {5, {75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99}, "ORANGE", RED_DIM, RED}
};

game_state state = {
  0,
  false,
  
  {},
  NO_BUTTON,
  NO_BUTTON,
  {},
  showing_pattern,
  0,
  0
};

const char* message = "rob rules";
bool say_my_name = true;
unsigned long state_shown_ms = 0;

void show_debug_info() {
  if (state.current_loop_start_ms - state_shown_ms > 500) {
    Serial.println("Current State");
    if (say_my_name) {
      Serial.println(message);
    }
    Serial.println("-------------");
    Serial.print("Pressed Button: ");Serial.println(state.active_button);
    Serial.print("Lifted Button: ");Serial.println(state.lifted_button);
    Serial.print("pattern: ");
    for (int i = 0; i < MAX_PATTERN_LENGTH && state.pattern[i] != PATTERN_TERMINATOR; i++) {
      if (i == state.current_step) {
        Serial.print("*");
      }
      Serial.print(state.pattern[i]);
      if (i == state.current_step) {
        Serial.print("*");
      }
      Serial.print(" ");
    }
    Serial.println("");
    Serial.println("-------------");
    state_shown_ms = millis();
  }
}

void set_current_step(uint8_t step) {
  state.current_step = step;
  state.current_step_since_ms = millis();
  state.current_loop_did_update_state = true;
}

void setup() {
  Serial.begin(9600);
  strip.setBrightness(128);
  strip.begin();
  randomSeed(analogRead(UNCONNECTED_ANALOG_PIN));

  for (int i = 0; i < BUTTON_CT; i++) {
    pinMode(BUTTONS[i].btn_pin, INPUT);
    state.buttons_pressed_state[i] = false;
  }

  state.pattern[0] = random(BUTTON_CT);
  for (int i = 1; i < MAX_PATTERN_LENGTH; i++) {
    state.pattern[i] = PATTERN_TERMINATOR;
  }

  set_current_step(0);

  strip.show();

  while (!Serial) { delay(10); }
  Serial.println("initialized");
}

void update_button_state() {
  bool any_button_pressed = false;
  for (int i = 0; i < BUTTON_CT; i++) {
    bool pressed_state = digitalRead(BUTTONS[i].btn_pin) == HIGH;
    if (pressed_state) {
      any_button_pressed = true;
    }
    if (state.buttons_pressed_state[i] != pressed_state) {
      state.buttons_pressed_state[i] = pressed_state;
      if (pressed_state && state.active_button == NO_BUTTON) {
        state.active_button = i;
      }
      state.current_loop_did_update_state = true;
    }
  }
  state.lifted_button = NO_BUTTON;
  if (!any_button_pressed) {
    state.lifted_button = state.active_button;
    state.active_button = NO_BUTTON;
    // if any of these changed, the loop above already updated `current_loop_did_update_state`
  }
}

void update_game_state() {
  if (state.current_mode == showing_pattern) {
    if (state.current_loop_start_ms - state.current_step_since_ms > PAUSE_BEFORE_LIGHT_MS + SHOW_LIGHT_MS + PAUSE_AFTER_LIGHT_MS) {
      // we've shown the light, and we've paused long enough after showing it
      // time to go to the next step in the pattern if there are any remaining
      if (state.pattern[state.current_step + 1] != PATTERN_TERMINATOR) {
        set_current_step(state.current_step + 1);
      } else {
        // no more steps left in the pattern, it's time for the players to play!
        state.current_mode = awaiting_input;
        set_current_step(0);  // already sets current_loop_did_update_state for us
      }
    }
  } else {
    if (state.lifted_button == state.pattern[state.current_step]) {
      // correct
      // advance to the next step if possible, or add to the pattern and switch modes
      if (state.current_step == MAX_PATTERN_LENGTH) {
        Serial.println("you won!");
      } else if (state.pattern[state.current_step + 1] != PATTERN_TERMINATOR) {
        set_current_step(state.current_step + 1);
      } else {
        state.pattern[state.current_step + 1] = random(BUTTON_CT);
        state.current_mode = showing_pattern;
        set_current_step(0);
      }
    } else if (state.active_button != NO_BUTTON && state.active_button != state.pattern[state.current_step]) {
      Serial.println("game over");
    } else {
    }
  }
}

void refresh_lights() {
  if (!state.current_loop_did_update_state) {
    // if nothing's changed, we won't need to update any lights
    //return;
  }
  
  for (int i = 0; i < BUTTON_CT; i++) {
    uint32_t color = BUTTONS[i].dim_color;
    if (state.current_mode == awaiting_input && state.active_button == i) {
      color = BUTTONS[i].color;
    } else if (state.current_mode == showing_pattern) {
      if (
          state.pattern[state.current_step] == i &&
          state.current_loop_start_ms - state.current_step_since_ms < SHOW_LIGHT_MS + PAUSE_BEFORE_LIGHT_MS &&
          state.current_loop_start_ms - state.current_step_since_ms > PAUSE_BEFORE_LIGHT_MS
      ) {
        color = BUTTONS[state.pattern[state.current_step]].color;
      }
    }
    for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
      strip.setPixelColor(BUTTONS[i].led_nums[j], color);
    }
  }
  strip.show();
}

void loop() {
  state.current_loop_start_ms = millis();
  state.current_loop_did_update_state = false;

  // must be called before other updates, to get new user input for handling
  update_button_state();

  update_game_state();

  refresh_lights();
  
  show_debug_info();
}
