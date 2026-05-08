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
static void configureFordPip(TriggerWaveform * s, size_t count) {
	s->initialize(FOUR_STROKE_CAM_SENSOR, SyncEdge::Rise);

	s->tdcPosition = 662.5;

	s->setTriggerSynchronizationGap(0.66);
	s->setSecondTriggerSynchronizationGap(1.25);
	/**
	 * sensor is mounted on distributor but trigger shape is defined in engine cycle angles
	 */
	int oneCylinder = s->getCycleDuration() / count;

	s->addEventAngle(oneCylinder * 0.75, TriggerValue::RISE);
	s->addEventAngle(oneCylinder, TriggerValue::FALL);


	for (int i = 2;i<=count;i++) {
		s->addEventAngle(oneCylinder * (i - 0.5), TriggerValue::RISE);
		s->addEventAngle(oneCylinder * i, TriggerValue::FALL);
	}

}

void configureFordPip6(TriggerWaveform * s) {
	/*
	 * Ford TFI PIP - Inline 6 cylinder (4.9L 300 I6 EEC-IV)
	 *
	 * Physical wheel sequence: short tooth -> long gap -> 5 normal teeth_
	 * Original rusefi code had: long gap -> short tooth -> 5 normal teeth
	 *
	 * Short tooth is placed at end of cycle (690-720 deg), long gap
	 * wraps from 720 back to 90 deg where first normal tooth rises.
	 *
	 * Fall-to-fall sync ratios:
	 *   Short tooth FALL (720) to Tooth 1 FALL (150) = 150 deg  LONG
	 *   Normal tooth FALL to next FALL               = 120 deg  normal
	 */
	s->initialize(FOUR_STROKE_CAM_SENSOR, SyncEdge::Fall);
	s->tdcPosition = 662.5;

	s->setTriggerSynchronizationGap(0.55, 0.95);
	s->setSecondTriggerSynchronizationGap(1.45, 1.85);

	// Teeth 1-5: normal teeth, 60 deg wide, 60 deg gaps
	// Tooth 1 follows 90 deg long gap wrapping from previous cycle
	s->addEventAngle( 90, TriggerValue::RISE);
	s->addEventAngle(150, TriggerValue::FALL);

	s->addEventAngle(210, TriggerValue::RISE);
	s->addEventAngle(270, TriggerValue::FALL);

	s->addEventAngle(330, TriggerValue::RISE);
	s->addEventAngle(390, TriggerValue::FALL);

	s->addEventAngle(450, TriggerValue::RISE);
	s->addEventAngle(510, TriggerValue::FALL);

	s->addEventAngle(570, TriggerValue::RISE);
	s->addEventAngle(630, TriggerValue::FALL);

	// Short sync tooth: 30 deg wide, long gap follows (wraps to next cycle)
	s->addEventAngle(690, TriggerValue::RISE);
	s->addEventAngle(720, TriggerValue::FALL);
}

void configureFordPip8(TriggerWaveform * s) {
	configureFordPip(s, 8);
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
