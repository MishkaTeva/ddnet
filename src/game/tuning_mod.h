// F-DDrace mod tuning parameters — appended on top of DDNet tuning.h.
// When CONF_FDDRACE_MOD is undefined this file contributes no parameters.

#ifndef CONF_FDDRACE_MOD
// Mod build disabled — no extra tuning params.
#else

MACRO_TUNING_PARAM(TaserFireDelay, taser_fire_delay, 800, "Delay of firing taser")
MACRO_TUNING_PARAM(PortalRifleFireDelay, portalrifle_fire_delay, 125, "Delay of using portal rifle")
MACRO_TUNING_PARAM(PlasmaRifleFireDelay, plasma_rifle_fire_delay, 500, "Delay of firing plasma rifle")
MACRO_TUNING_PARAM(ProjectileRifleFireDelay, projectile_rifle_fire_delay, 100, "Delay of firing projectile rifle")
MACRO_TUNING_PARAM(TeleRifleFireDelay, telerifle_fire_delay, 250, "Delay of using tele rifle")
MACRO_TUNING_PARAM(LightningLaserFireDelay, lightning_laser_fire_delay, 110, "Delay of using lightning laser")
MACRO_TUNING_PARAM(StraightGrenadeFireDelay, straight_grenade_fire_delay, 500, "Delay of firing straight grenade")
MACRO_TUNING_PARAM(BallGrenadeFireDelay, ball_grenade_fire_delay, 600, "Delay of firing ball grenade")
MACRO_TUNING_PARAM(HeartGunFireDelay, heart_gun_fire_delay, 125, "Delay of firing heart gun")
MACRO_TUNING_PARAM(LightsaberFireDelay, lightsaber_fire_delay, 500, "Delay of using lightsaber")
MACRO_TUNING_PARAM(TelekinesisFireDelay, telekinesis_fire_delay, 125, "Delay of using telekinesis")
MACRO_TUNING_PARAM(EditorFireDelay, editor_fire_delay, 0, "UNUSED")
MACRO_TUNING_PARAM(PortalBlockerFireDelay, portal_blocker_fire_delay, 125, "Delay of using portal blocker")

MACRO_TUNING_PARAM(StraightGrenadeSpeed, straight_grenade_speed, 750.0f, "Straight grenade speed")
MACRO_TUNING_PARAM(StraightGrenadeLifetime, straight_grenade_lifetime, 5.0f, "Straight grenade lifetime")

MACRO_TUNING_PARAM(VanillaShotgunCurvature, vanilla_shotgun_curvature, 1.25f, "Vanilla shotgun curvature")
MACRO_TUNING_PARAM(VanillaShotgunSpeed, vanilla_shotgun_speed, 2750.0f, "Vanilla shotgun speed")

MACRO_TUNING_PARAM(VanillaGunCurvature, vanilla_gun_curvature, 1.25f, "Vanilla gun curvature")
MACRO_TUNING_PARAM(VanillaGunSpeed, vanilla_gun_speed, 2200.0f, "Vanilla gun speed")
MACRO_TUNING_PARAM(VanillaGunLifetime, vanilla_gun_lifetime, 2.0f, "Vanilla gun lifetime")

MACRO_TUNING_PARAM(MeteorFriction, meteor_friction, 5000, "Meteor friction")
MACRO_TUNING_PARAM(MeteorMaxAccel, meteor_max_accel, 2000, "Max meteor acceleration per player in pixel/tick^2")
MACRO_TUNING_PARAM(MeteorAccelPreserve, meteor_accel_preserve, 100000, "How much acceleration is preserved with growing distance to the player")

MACRO_TUNING_PARAM(MoneyMaxFlySpeed, money_max_fly_speed, 3, "Fly speed for following or merging money drops")

MACRO_TUNING_PARAM(LightningLaserCount, lightning_laser_count, 7, "Number of lasers used for the lightning laser (min: 1)")
MACRO_TUNING_PARAM(LightningLaserLength, lightning_laser_length, 70, "Length of lasers used for the lightning laser (min: 1)")

MACRO_TUNING_PARAM(Elasticity, elasticity, 0, "Elasticity, bouncing off of blocks")
MACRO_TUNING_PARAM(NumSpreadShots, num_spread_shots, 3, "Number of shots for the spread weapons")

#endif
