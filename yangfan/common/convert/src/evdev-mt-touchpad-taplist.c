/*
# Copyright (c) 2020-2030 iSoftStone Information Technology (Group) Co.,Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
*/

#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "evdev-mt-touchpad.h"

#define DEFAULT_TAP_TIMEOUT_PERIOD ms2us(180)
#define DEFAULT_DRAG_TIMEOUT_PERIOD ms2us(300)
#define DEFAULT_TAP_MOVE_THRESHOLD 1.3 /* mm */

enum tap_event {
	TAP_EVENT_TOUCH = 12,
	TAP_EVENT_MOTION,
	TAP_EVENT_RELEASE,
	TAP_EVENT_BUTTON,
	TAP_EVENT_TIMEOUT,
	TAP_EVENT_THUMB,
	TAP_EVENT_PALM,
	TAP_EVENT_PALM_UP,
};

static inline const char*
tap_state_to_str(enum tp_tap_state state)
{
	switch(state) {
	CASE_RETURN_STRING(TAP_STATE_IDLE);
	CASE_RETURN_STRING(TAP_STATE_HOLD);
	CASE_RETURN_STRING(TAP_STATE_TOUCH);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_TAPPED);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_TAPPED);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_TAPPED);
	CASE_RETURN_STRING(TAP_STATE_TOUCH_2);
	CASE_RETURN_STRING(TAP_STATE_TOUCH_2_HOLD);
	CASE_RETURN_STRING(TAP_STATE_TOUCH_2_RELEASE);
	CASE_RETURN_STRING(TAP_STATE_TOUCH_3);
	CASE_RETURN_STRING(TAP_STATE_TOUCH_3_HOLD);
	CASE_RETURN_STRING(TAP_STATE_TOUCH_3_RELEASE);
	CASE_RETURN_STRING(TAP_STATE_TOUCH_3_RELEASE_2);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING_WAIT);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING_WAIT);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING_WAIT);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING_OR_DOUBLETAP);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING_OR_DOUBLETAP);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING_OR_DOUBLETAP);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING_OR_DOUBLETAP_2);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING_OR_DOUBLETAP_2);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING_OR_DOUBLETAP_2);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING_OR_DOUBLETAP_2_RELEASE);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING_OR_DOUBLETAP_2_RELEASE);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING_OR_DOUBLETAP_2_RELEASE);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING_OR_TAP);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING_OR_TAP);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING_OR_TAP);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING_OR_TAP_2);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING_OR_TAP_2);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING_OR_TAP_2);
	CASE_RETURN_STRING(TAP_STATE_1FGTAP_DRAGGING_2);
	CASE_RETURN_STRING(TAP_STATE_2FGTAP_DRAGGING_2);
	CASE_RETURN_STRING(TAP_STATE_3FGTAP_DRAGGING_2);
	CASE_RETURN_STRING(TAP_STATE_DEAD);
	}
	return NULL;
}

static inline const char*
tap_event_to_str(enum tap_event event)
{
	switch(event) {
	CASE_RETURN_STRING(TAP_EVENT_TOUCH);
	CASE_RETURN_STRING(TAP_EVENT_MOTION);
	CASE_RETURN_STRING(TAP_EVENT_RELEASE);
	CASE_RETURN_STRING(TAP_EVENT_TIMEOUT);
	CASE_RETURN_STRING(TAP_EVENT_BUTTON);
	CASE_RETURN_STRING(TAP_EVENT_THUMB);
	CASE_RETURN_STRING(TAP_EVENT_PALM);
	CASE_RETURN_STRING(TAP_EVENT_PALM_UP);
	}
	return NULL;
}

static inline void
log_tap_bug(struct tp_dispatch *tp, struct tp_touch *t, enum tap_event event)
{
	evdev_log_bug_libinput(tp->device,
			       "%d: invalid tap event %s in state %s\n",
			       t->index,
			       tap_event_to_str(event),
			       tap_state_to_str(tp->tap.state));

}

static void
tp_tap_notify(struct tp_dispatch *tp,
	      uint64_t time,
	      int nfingers,
	      enum libinput_button_state state)
{
	int32_t button;
	int32_t button_map[2][3] = {
		{ BTN_LEFT, BTN_RIGHT, BTN_MIDDLE },
		{ BTN_LEFT, BTN_MIDDLE, BTN_RIGHT },
	};

	assert(tp->tap.map < ARRAY_LENGTH(button_map));

	if (nfingers < 1 || nfingers > 3)
		return;

	button = button_map[tp->tap.map][nfingers - 1];

	if (state == LIBINPUT_BUTTON_STATE_PRESSED)
		tp->tap.buttons_pressed |= (1 << nfingers);
	else
		tp->tap.buttons_pressed &= ~(1 << nfingers);

	evdev_pointer_notify_button(tp->device,
				    time,
				    button,
				    state);
}

static void
tp_tap_set_timer(struct tp_dispatch *tp, uint64_t time)
{
	libinput_timer_set(&tp->tap.timer, time + DEFAULT_TAP_TIMEOUT_PERIOD);
}

static void
tp_tap_set_drag_timer(struct tp_dispatch *tp, uint64_t time)
{
	libinput_timer_set(&tp->tap.timer, time + DEFAULT_DRAG_TIMEOUT_PERIOD);
}

static void
tp_tap_clear_timer(struct tp_dispatch *tp)
{
	libinput_timer_cancel(&tp->tap.timer);
}

static void
tp_tap_move_to_dead(struct tp_dispatch *tp, struct tp_touch *t)
{
	tp->tap.state = TAP_STATE_DEAD;
	t->tap.state = TAP_TOUCH_STATE_DEAD;
	tp_tap_clear_timer(tp);
}

static void
tp_tap_idle_handle_event(struct tp_dispatch *tp,
			 struct tp_touch *t,
			 enum tap_event event, uint64_t time)
{
	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_TOUCH;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		break;
	case TAP_EVENT_MOTION:
		log_tap_bug(tp, t, event);
		break;
	case TAP_EVENT_TIMEOUT:
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		log_tap_bug(tp, t, event);
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_IDLE;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch_handle_event(struct tp_dispatch *tp,
			  struct tp_touch *t,
			  enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_TOUCH_2;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      1,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		if (tp->tap.drag_enabled) {
			tp->tap.state = TAP_STATE_1FGTAP_TAPPED;
			tp->tap.saved_release_time = time;
			tp_tap_set_timer(tp, time);
		} else {
			tp_tap_notify(tp,
				      time,
				      1,
				      LIBINPUT_BUTTON_STATE_RELEASED);
			tp->tap.state = TAP_STATE_IDLE;
		}
		break;
	case TAP_EVENT_MOTION:
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		tp->tap.state = TAP_STATE_HOLD;
		tp_tap_clear_timer(tp);
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		tp->tap.state = TAP_STATE_IDLE;
		t->tap.is_thumb = true;
		tp->tap.nfingers_down--;
		t->tap.state = TAP_TOUCH_STATE_DEAD;
		tp_tap_clear_timer(tp);
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_IDLE;
		tp_tap_clear_timer(tp);
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_hold_handle_event(struct tp_dispatch *tp,
			 struct tp_touch *t,
			 enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_TOUCH_2;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_IDLE;
		break;
	case TAP_EVENT_MOTION:
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		tp->tap.state = TAP_STATE_IDLE;
		t->tap.is_thumb = true;
		tp->tap.nfingers_down--;
		t->tap.state = TAP_TOUCH_STATE_DEAD;
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_IDLE;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_tapped_handle_event(struct tp_dispatch *tp,
			   struct tp_touch *t,
			   enum tap_event event, uint64_t time,
			   int nfingers_tapped)
{
	switch (event) {
	case TAP_EVENT_MOTION:
	case TAP_EVENT_RELEASE:
		log_tap_bug(tp, t, event);
		break;
	case TAP_EVENT_TOUCH: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING_OR_DOUBLETAP,
			TAP_STATE_2FGTAP_DRAGGING_OR_DOUBLETAP,
			TAP_STATE_3FGTAP_DRAGGING_OR_DOUBLETAP,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	}
	case TAP_EVENT_TIMEOUT:
		tp->tap.state = TAP_STATE_IDLE;
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case TAP_EVENT_THUMB:
		log_tap_bug(tp, t, event);
		break;
	case TAP_EVENT_PALM:
		log_tap_bug(tp, t, event);
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch2_handle_event(struct tp_dispatch *tp,
			   struct tp_touch *t,
			   enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_TOUCH_3;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_TOUCH_2_RELEASE;
		tp->tap.saved_release_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_MOTION:
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		tp->tap.state = TAP_STATE_TOUCH_2_HOLD;
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_TOUCH;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch2_hold_handle_event(struct tp_dispatch *tp,
				struct tp_touch *t,
				enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_TOUCH_3;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_HOLD;
		break;
	case TAP_EVENT_MOTION:
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		tp->tap.state = TAP_STATE_TOUCH_2_HOLD;
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_HOLD;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch2_release_handle_event(struct tp_dispatch *tp,
				   struct tp_touch *t,
				   enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_TOUCH_2_HOLD;
		t->tap.state = TAP_TOUCH_STATE_DEAD;
		tp_tap_clear_timer(tp);
		break;
	case TAP_EVENT_RELEASE:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      2,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		if (tp->tap.drag_enabled) {
			tp->tap.state = TAP_STATE_2FGTAP_TAPPED;
			tp_tap_set_timer(tp, time);
		} else {
			tp_tap_notify(tp,
				      tp->tap.saved_release_time,
				      2,
				      LIBINPUT_BUTTON_STATE_RELEASED);
			tp->tap.state = TAP_STATE_IDLE;
		}
		break;
	case TAP_EVENT_MOTION:
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		tp->tap.state = TAP_STATE_HOLD;
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		/* There's only one saved press time and it's overwritten by
		 * the last touch down. So in the case of finger down, palm
		 * down, finger up, palm detected, we use the
		 * palm touch's press time here instead of the finger's press
		 * time. Let's wait and see if that's an issue.
		 */
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      1,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		if (tp->tap.drag_enabled) {
			tp->tap.state = TAP_STATE_1FGTAP_TAPPED;
		} else {
			tp_tap_notify(tp,
				      tp->tap.saved_release_time,
				      1,
				      LIBINPUT_BUTTON_STATE_RELEASED);
			tp->tap.state = TAP_STATE_IDLE;
		}
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch3_handle_event(struct tp_dispatch *tp,
			   struct tp_touch *t,
			   enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_DEAD;
		tp_tap_clear_timer(tp);
		break;
	case TAP_EVENT_MOTION:
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		tp->tap.state = TAP_STATE_TOUCH_3_HOLD;
		tp_tap_clear_timer(tp);
		break;
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_TOUCH_3_RELEASE;
		tp->tap.saved_release_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_TOUCH_2;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch3_hold_handle_event(struct tp_dispatch *tp,
				struct tp_touch *t,
				enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp->tap.state = TAP_STATE_DEAD;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_TOUCH_2_HOLD;
		break;
	case TAP_EVENT_MOTION:
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_TOUCH_2_HOLD;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch3_release_handle_event(struct tp_dispatch *tp,
				   struct tp_touch *t,
				   enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_TOUCH_3;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_TOUCH_3_RELEASE_2;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_MOTION:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_TOUCH_2_HOLD;
		break;
	case TAP_EVENT_BUTTON:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_TOUCH_2_RELEASE;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_touch3_release2_handle_event(struct tp_dispatch *tp,
				    struct tp_touch *t,
				    enum tap_event event, uint64_t time)
{

	switch (event) {
	case TAP_EVENT_TOUCH:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_TOUCH_2;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		if (tp->tap.drag_enabled) {
			tp->tap.state = TAP_STATE_3FGTAP_TAPPED;
			tp_tap_set_timer(tp, time);
		} else {
			tp_tap_notify(tp,
				      tp->tap.saved_release_time,
				      3,
				      LIBINPUT_BUTTON_STATE_RELEASED);
			tp->tap.state = TAP_STATE_IDLE;
		}
		break;
	case TAP_EVENT_MOTION:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp_tap_move_to_dead(tp, t);
		break;
	case TAP_EVENT_TIMEOUT:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_HOLD;
		break;
	case TAP_EVENT_BUTTON:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      3,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      3,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_DEAD;
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      2,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		if (tp->tap.drag_enabled) {
			tp->tap.state = TAP_STATE_2FGTAP_TAPPED;
		} else {
			tp_tap_notify(tp,
				      tp->tap.saved_release_time,
				      2,
				      LIBINPUT_BUTTON_STATE_RELEASED);
			tp->tap.state = TAP_STATE_IDLE;
		}
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_dragging_or_doubletap_handle_event(struct tp_dispatch *tp,
					  struct tp_touch *t,
					  enum tap_event event, uint64_t time,
					  int nfingers_tapped)
{
	switch (event) {
	case TAP_EVENT_TOUCH: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING_OR_DOUBLETAP_2,
			TAP_STATE_2FGTAP_DRAGGING_OR_DOUBLETAP_2,
			TAP_STATE_3FGTAP_DRAGGING_OR_DOUBLETAP_2,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	}
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_1FGTAP_TAPPED;
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      1,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp->tap.saved_release_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_MOTION:
	case TAP_EVENT_TIMEOUT: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING,
			TAP_STATE_2FGTAP_DRAGGING,
			TAP_STATE_3FGTAP_DRAGGING,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		break;
	}
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_TAPPED,
			TAP_STATE_2FGTAP_TAPPED,
			TAP_STATE_3FGTAP_TAPPED,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		break;
	}
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_dragging_or_doubletap_2_handle_event(struct tp_dispatch *tp,
					    struct tp_touch *t,
					    enum tap_event event, uint64_t time,
					    int nfingers_tapped)
{
	switch (event) {
	case TAP_EVENT_TOUCH:
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_TOUCH_3;
		tp->tap.saved_press_time = time;
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_RELEASE: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING_OR_DOUBLETAP_2_RELEASE,
			TAP_STATE_2FGTAP_DRAGGING_OR_DOUBLETAP_2_RELEASE,
			TAP_STATE_3FGTAP_DRAGGING_OR_DOUBLETAP_2_RELEASE,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		/* We are overwriting saved_release_time, but if this is indeed
		   a multitap with two fingers, then we will need its previous
		   value for the click release event we withheld just in case
		   this is still a drag. */
		tp->tap.saved_multitap_release_time = tp->tap.saved_release_time;
		tp->tap.saved_release_time = time;
		tp_tap_set_timer(tp, time);
		break;
	}
	case TAP_EVENT_MOTION:
	case TAP_EVENT_TIMEOUT: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING_2,
			TAP_STATE_2FGTAP_DRAGGING_2,
			TAP_STATE_3FGTAP_DRAGGING_2,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		break;
	}
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING_OR_DOUBLETAP,
			TAP_STATE_2FGTAP_DRAGGING_OR_DOUBLETAP,
			TAP_STATE_3FGTAP_DRAGGING_OR_DOUBLETAP,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		break;
	}
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_dragging_or_doubletap_2_release_handle_event(struct tp_dispatch *tp,
						    struct tp_touch *t,
						    enum tap_event event,
						    uint64_t time,
						    int nfingers_tapped)
{
	switch (event) {
	case TAP_EVENT_TOUCH: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING_2,
			TAP_STATE_2FGTAP_DRAGGING_2,
			TAP_STATE_3FGTAP_DRAGGING_2,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		break;
	}
	case TAP_EVENT_RELEASE:
		tp->tap.state = TAP_STATE_2FGTAP_TAPPED;
		tp_tap_notify(tp,
			      tp->tap.saved_multitap_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      2,
			      LIBINPUT_BUTTON_STATE_PRESSED);
		tp_tap_set_timer(tp, time);
		break;
	case TAP_EVENT_MOTION:
	case TAP_EVENT_TIMEOUT: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING,
			TAP_STATE_2FGTAP_DRAGGING,
			TAP_STATE_3FGTAP_DRAGGING,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		break;
	}
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp->tap.state = TAP_STATE_1FGTAP_TAPPED;
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp_tap_notify(tp,
			      tp->tap.saved_press_time,
			      1,
			      LIBINPUT_BUTTON_STATE_PRESSED);
	case TAP_EVENT_PALM_UP:
		break;
	}
}

static void
tp_tap_dragging_handle_event(struct tp_dispatch *tp,
			     struct tp_touch *t,
			     enum tap_event event, uint64_t time,
			     int nfingers_tapped)
{

	switch (event) {
	case TAP_EVENT_TOUCH: {
		enum tp_tap_state dest[3] = {
			TAP_STATE_1FGTAP_DRAGGING_2,
			TAP_STATE_2FGTAP_DRAGGING_2,
			TAP_STATE_3FGTAP_DRAGGING_2,
		};
		assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
		tp->tap.state = dest[nfingers_tapped - 1];
		break;
	}
	case TAP_EVENT_RELEASE:
		if (tp->tap.drag_lock_enabled) {
			enum tp_tap_state dest[3] = {
				TAP_STATE_1FGTAP_DRAGGING_WAIT,
				TAP_STATE_2FGTAP_DRAGGING_WAIT,
				TAP_STATE_3FGTAP_DRAGGING_WAIT,
			};
			assert(nfingers_tapped >= 1 && nfingers_tapped <= 3);
			tp->tap.state = dest[nfingers_tapped - 1];
			tp_tap_set_drag_timer(tp, time);
		} else {
			tp_tap_notify(tp,
				      time,
				      nfingers_tapped,
				      LIBINPUT_BUTTON_STATE_RELEASED);
			tp->tap.state = TAP_STATE_IDLE;
		}
		break;
	case TAP_EVENT_MOTION:
	case TAP_EVENT_TIMEOUT:
		/* noop */
		break;
	case TAP_EVENT_BUTTON:
		tp->tap.state = TAP_STATE_DEAD;
		tp_tap_notify(tp,
			      time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case TAP_EVENT_THUMB:
		break;
	case TAP_EVENT_PALM:
		tp_tap_notify(tp,
			      tp->tap.saved_release_time,
			      nfingers_tapped,
			      LIBINPUT_BUTTON_STATE_RELEASED);
		tp->tap.state = TAP_STATE_IDLE;
		break;
	case TAP_EVENT_PALM_UP:
		break;
	}
}