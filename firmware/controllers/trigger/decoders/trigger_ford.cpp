/**
 * @file	trigger_ford.cpp
 *
 * @author Andrey Belomutskiy, (c) rusEFI LLC 2012-2023
 */

#include "pch.h"

#include "trigger_ford.h"

/**
 * based on https://fordsix.com/threads/understanding-standard-and-signature-pip-thick-film-ignition.81515/
 * based on https://www.w8ji.com/distributor_stabbing.htm
 */
void configureFordPip8(TriggerWaveform * s) {
	s->initialize(FOUR_STROKE_CAM_SENSOR, SyncEdge::Fall);

	s->tdcPosition = 662.5;

    // Sync on falling edges. Fall-to-fall intervals: 90 x6, 76.5 (short signature tooth), 103.5 (long gap).
    // Ratios (current/previous): Fall 8 (720) = 76.5/90 = 0.85, Fall 1 (103.5) = 103.5/76.5 = 1.353
    // Sync at Fall 1: gap1 must contain 1.353, gap2 (previous) must contain 0.85.
	s->setTriggerSynchronizationGap2(1.15, 1.75);
	s->setSecondTriggerSynchronizationGap2(0.50, 1.10);

	// Tooth 1: Normal tooth after 63 deg gap
	s->addEventAngle(58.5, TriggerValue::RISE);
	s->addEventAngle(103.5, TriggerValue::FALL);

	// Tooth 2
	s->addEventAngle(148.5, TriggerValue::RISE);
	s->addEventAngle(193.5, TriggerValue::FALL);

	// Tooth 3
	s->addEventAngle(238.5, TriggerValue::RISE);
	s->addEventAngle(283.5, TriggerValue::FALL);

	// Tooth 4
	s->addEventAngle(328.5, TriggerValue::RISE);
	s->addEventAngle(373.5, TriggerValue::FALL);

	// Tooth 5
	s->addEventAngle(418.5, TriggerValue::RISE);
	s->addEventAngle(463.5, TriggerValue::FALL);

	// Tooth 6
	s->addEventAngle(508.5, TriggerValue::RISE);
	s->addEventAngle(553.5, TriggerValue::FALL);

	// Tooth 7
	s->addEventAngle(598.5, TriggerValue::RISE);
	s->addEventAngle(643.5, TriggerValue::FALL);

	// Tooth 8: Short signature tooth
	s->addEventAngle(688.5, TriggerValue::RISE);
	s->addEventAngle(720.0, TriggerValue::FALL);
}

void configureFordPip6(TriggerWaveform * s) {
	/*
	 * Ford TFI PIP - Inline 6 cylinder (4.9L 300 I6 EEC-IV)
	 *
	 * Physical wheel sequence:
	 * Tooth 1: 60 deg tooth after the long gap (Rise: 85, Fall: 145)
	 * Teeth 2-5: 60 deg teeth with 60 deg gaps
	 * Tooth 6: short sync tooth (Rise: 685, Fall: 720)
	 *
	 * The tooth 6 FALL was originally modelled at 720. Datalog
	 * 2026-09-20_21.57.52 (1995 F150, 935-2804 RPM, 0 trigger errors)
	 * shows the five normal falls on a clean 120 deg grid but the
	 * tooth 5 FALL -> tooth 6 FALL interval at ~95 deg (not 102) and
	 * tooth 6 FALL -> tooth 1 FALL at ~145 deg (not 138): the sync
	 * tooth's falling edge is ~7 deg early relative to the old layout. With the
	 * old layout, instant RPM showed a fixed +7%/-5% per-tooth ripple and
	 * events scheduled off this tooth (0-78 deg window) fired ~7 deg early.
	 *
	 * rusEFI requires the last event of a shape to sit at exactly 720
	 * (checkSwitchTimes), so instead of moving the tooth 6 fall to 713 the
	 * whole wheel is rotated +7 deg and tdcPosition moves +7 deg with it;
	 * the physical timing reference is unchanged.
	 *
	 * Measured fall-to-fall ratios (current interval / previous interval):
	 * Tooth 1 FALL (145):  145 / 95  = 1.52  (LONG)   second sync gap
	 * Tooth 2 FALL (265):  120 / 145 = 0.83  (SHORT)  primary sync gap -> sync point
	 * Tooth 6 FALL (720):   95 / 120 = 0.79  (SHORT)  in primary window, rejected by second gap (1.0)
	 */
	s->initialize(FOUR_STROKE_CAM_SENSOR, SyncEdge::Fall);
	s->tdcPosition = 669.5;

	// Windows sized around the measured ratios (0.83 sync, 0.79 tooth 6, 1.52 long gap)
	s->setTriggerSynchronizationGap2(0.70, 0.95);
	s->setSecondTriggerSynchronizationGap2(1.20, 1.70);

	// Tooth 1: Normal tooth after the long gap
	s->addEventAngle( 85, TriggerValue::RISE);
	s->addEventAngle(145, TriggerValue::FALL);

	// Tooth 2
	s->addEventAngle(205, TriggerValue::RISE);
	s->addEventAngle(265, TriggerValue::FALL);

	// Tooth 3
	s->addEventAngle(325, TriggerValue::RISE);
	s->addEventAngle(385, TriggerValue::FALL);

	// Tooth 4
	s->addEventAngle(445, TriggerValue::RISE);
	s->addEventAngle(505, TriggerValue::FALL);

	// Tooth 5
	s->addEventAngle(565, TriggerValue::RISE);
	s->addEventAngle(625, TriggerValue::FALL);

	// Tooth 6: Short sync tooth (~35 deg wide)
	s->addEventAngle(685, TriggerValue::RISE);
	s->addEventAngle(720, TriggerValue::FALL);
}

void configureFordST170(TriggerWaveform * s) {
	s->initialize(FOUR_STROKE_CAM_SENSOR, SyncEdge::RiseOnly);
	int width = 10;

	int total = s->getCycleDuration() / 8;

	s->addEventAngle(1 * total - width, TriggerValue::RISE);
	s->addEventAngle(1 * total, TriggerValue::FALL);

	s->addEventAngle(2 * total - width, TriggerValue::RISE);
	s->addEventAngle(2 * total, TriggerValue::FALL);

	s->addEventAngle(4 * total - width, TriggerValue::RISE);
	s->addEventAngle(4 * total, TriggerValue::FALL);

	s->addEventAngle(6 * total - width, TriggerValue::RISE);
	s->addEventAngle(6 * total, TriggerValue::FALL);

	s->addEventAngle(8 * total - width, TriggerValue::RISE);
	s->addEventAngle(8 * total, TriggerValue::FALL);
}

void configureFordCoyote(TriggerWaveform *s) {
	static const angle_t angles[] = { 45, 90, 180 - 30, 180, 270 - 30, 270, 360 };
	initializeRiseOnlyTrigger(s, 10, angles, efi::size(angles));

	s->setTriggerSynchronizationGap(3);
	s->setSecondTriggerSynchronizationGap(0.5);
}
