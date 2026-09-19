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
	s->initialize(FOUR_STROKE_CAM_SENSOR, SyncEdge::Rise);

	s->tdcPosition = 662.5;

	// Synchronization gaps matched to your Excel Fall-to-Fall ratios:
	// Fall 7 to Fall 8 (76.5 / 90) = 0.85
	// Fall 8 to Fall 1 (103.5 / 90) = 1.15
	s->setTriggerSynchronizationGap2(1.30, 1.70);
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
	 * Physical wheel sequence based on table data:
	 * Tooth 1: 60 deg tooth after 78 deg gap (Rise: 78, Fall: 138)
	 * Teeth 2-5: 60 deg teeth with 60 deg gaps
	 * Tooth 6: 42 deg short tooth (Rise: 678, Fall: 720)
	 *
	 * Sync Ratio Calculations (Fall-to-Fall):
	 * Tooth 1 FALL (138) / Tooth 6 FALL to Tooth 1 FALL (138 deg) = 1.3529 (LONG)
	 * Tooth 2 FALL (258) / Tooth 1 FALL to Tooth 2 FALL (120 deg) = 0.8696 (SHORT)
	 * Tooth 6 FALL (720) / Tooth 5 FALL to Tooth 6 FALL (102 deg) = 0.8500 (SHORT)
	 */
	s->initialize(FOUR_STROKE_CAM_SENSOR, SyncEdge::Rise);
	s->tdcPosition = 662.5;

	// Set synchronization gap ratios based on table fall-to-fall ratios (1.353, 0.870, 0.850)
	s->setTriggerSynchronizationGap2(0.70, 0.95);
	s->setSecondTriggerSynchronizationGap2(1.20, 1.70);

	// Tooth 1: Normal tooth after 78 deg gap
	s->addEventAngle( 78, TriggerValue::RISE);
	s->addEventAngle(138, TriggerValue::FALL);

	// Tooth 2
	s->addEventAngle(198, TriggerValue::RISE);
	s->addEventAngle(258, TriggerValue::FALL);

	// Tooth 3
	s->addEventAngle(318, TriggerValue::RISE);
	s->addEventAngle(378, TriggerValue::FALL);

	// Tooth 4
	s->addEventAngle(438, TriggerValue::RISE);
	s->addEventAngle(498, TriggerValue::FALL);

	// Tooth 5
	s->addEventAngle(558, TriggerValue::RISE);
	s->addEventAngle(618, TriggerValue::FALL);

	// Tooth 6: Short sync tooth (42 deg wide)
	s->addEventAngle(678, TriggerValue::RISE);
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
