extern crate systems;

mod air_conditioning;
mod airframe;
mod electrical;
mod fuel;
pub mod hydraulic;
mod navigation;
mod payload;
mod pneumatic;
mod power_consumption;

use self::{
    air_conditioning::{A330AirConditioning, A330PressurizationOverheadPanel},
    fuel::A330Fuel,
    payload::A330Payload,
    pneumatic::{A330Pneumatic, A330PneumaticOverheadPanel},
};
use airframe::A330Airframe;
use electrical::{
    A330Electrical, A330ElectricalOverheadPanel, A330EmergencyElectricalOverheadPanel,
    APU_START_MOTOR_BUS_TYPE,
};
use hydraulic::{A330Hydraulic, A330HydraulicOverheadPanel};
use navigation::A330RadioAltimeters;
use power_consumption::A330PowerConsumption;
use systems::enhanced_gpwc::EnhancedGroundProximityWarningComputer;
use systems::simulation::InitContext;
use uom::si::{f64::Length, length::nautical_mile};

use systems::{
    air_starter_unit::AirStarterUnit,
    apu::{
        Aps3200ApuGenerator, Aps3200Constants, Aps3200StartMotor, AuxiliaryPowerUnit,
        AuxiliaryPowerUnitFactory, AuxiliaryPowerUnitFireOverheadPanel,
        AuxiliaryPowerUnitOverheadPanel,
    },
    electrical::{Electricity, ElectricitySource, ExternalPowerSource},
    engine::{reverser_thrust::ReverserForce, trent_engine::TrentEngine, EngineFireOverheadPanel},
    hydraulic::brake_circuit::AutobrakePanel,
    landing_gear::{LandingGear, LandingGearControlInterfaceUnitSet},
    navigation::adirs::{
        AirDataInertialReferenceSystem, AirDataInertialReferenceSystemOverheadPanel,
    },
    shared::ElectricalBusType,
    simulation::{Aircraft, SimulationElement, SimulationElementVisitor, UpdateContext},
};

pub struct A330 {
    adirs: AirDataInertialReferenceSystem,
    adirs_overhead: AirDataInertialReferenceSystemOverheadPanel,
    air_conditioning: A330AirConditioning,
    apu: AuxiliaryPowerUnit<Aps3200ApuGenerator, Aps3200StartMotor, Aps3200Constants, 1>,
    asu: AirStarterUnit,
    apu_fire_overhead: AuxiliaryPowerUnitFireOverheadPanel,
    apu_overhead: AuxiliaryPowerUnitOverheadPanel,
    pneumatic_overhead: A330PneumaticOverheadPanel,
    pressurization_overhead: A330PressurizationOverheadPanel,
    electrical_overhead: A330ElectricalOverheadPanel,
    emergency_electrical_overhead: A330EmergencyElectricalOverheadPanel,
    payload: A330Payload,
    airframe: A330Airframe,
    fuel: A330Fuel,
    engine_1: TrentEngine,
    engine_2: TrentEngine,
    engine_fire_overhead: EngineFireOverheadPanel<2>,
    electrical: A330Electrical,
    power_consumption: A330PowerConsumption,
    ext_pwr: ExternalPowerSource,
    lgcius: LandingGearControlInterfaceUnitSet,
    hydraulic: A330Hydraulic,
    hydraulic_overhead: A330HydraulicOverheadPanel,
    autobrake_panel: AutobrakePanel,
    landing_gear: LandingGear,
    pneumatic: A330Pneumatic,
    radio_altimeters: A330RadioAltimeters,
    egpwc: EnhancedGroundProximityWarningComputer,
    reverse_thrust: ReverserForce,
}
impl A330 {
    pub fn new(context: &mut InitContext) -> A330 {
        A330 {
            adirs: AirDataInertialReferenceSystem::new(context),
            adirs_overhead: AirDataInertialReferenceSystemOverheadPanel::new(context),
            air_conditioning: A330AirConditioning::new(context),
            apu: AuxiliaryPowerUnitFactory::new_aps3200(
                context,
                1,
                APU_START_MOTOR_BUS_TYPE,
                ElectricalBusType::DirectCurrentBattery,
                ElectricalBusType::DirectCurrentBattery,
            ),
            asu: AirStarterUnit::new(context),
            apu_fire_overhead: AuxiliaryPowerUnitFireOverheadPanel::new(context),
            apu_overhead: AuxiliaryPowerUnitOverheadPanel::new(context),
            pneumatic_overhead: A330PneumaticOverheadPanel::new(context),
            pressurization_overhead: A330PressurizationOverheadPanel::new(context),
            electrical_overhead: A330ElectricalOverheadPanel::new(context),
            emergency_electrical_overhead: A330EmergencyElectricalOverheadPanel::new(context),
            payload: A330Payload::new(context),
            airframe: A330Airframe::new(context),
            fuel: A330Fuel::new(context),
            engine_1: TrentEngine::new(context, 1),
            engine_2: TrentEngine::new(context, 2),
            engine_fire_overhead: EngineFireOverheadPanel::new(context),
            electrical: A330Electrical::new(context),
            power_consumption: A330PowerConsumption::new(context),
            ext_pwr: ExternalPowerSource::new(context, 1),
            lgcius: LandingGearControlInterfaceUnitSet::new(
                context,
                ElectricalBusType::DirectCurrentEssential,
                ElectricalBusType::DirectCurrentGndFltService,
            ),
            hydraulic: A330Hydraulic::new(context),
            hydraulic_overhead: A330HydraulicOverheadPanel::new(context),
            autobrake_panel: AutobrakePanel::new(context),
            landing_gear: LandingGear::new(context),
            pneumatic: A330Pneumatic::new(context),
            radio_altimeters: A330RadioAltimeters::new(context),
            egpwc: EnhancedGroundProximityWarningComputer::new(
                context,
                ElectricalBusType::DirectCurrent(1),
                vec![
                    Length::new::<nautical_mile>(10.0),
                    Length::new::<nautical_mile>(20.0),
                    Length::new::<nautical_mile>(40.0),
                    Length::new::<nautical_mile>(80.0),
                    Length::new::<nautical_mile>(160.0),
                    Length::new::<nautical_mile>(320.0),
                ],
                0,
            ),
            reverse_thrust: ReverserForce::new(context),
        }
    }
}
impl Aircraft for A330 {
    fn update_before_power_distribution(
        &mut self,
        context: &UpdateContext,
        electricity: &mut Electricity,
    ) {
        self.apu.update_before_electrical(
            context,
            &self.apu_overhead,
            &self.apu_fire_overhead,
            self.pneumatic_overhead.apu_bleed_is_on(),
            // This will be replaced when integrating the whole electrical system.
            // For now we use the same logic as found in the JavaScript code; ignoring whether or not
            // the engine generators are supplying electricity.
            self.electrical_overhead.apu_generator_is_on()
                && !(self.electrical_overhead.external_power_is_on()
                    && self.electrical_overhead.external_power_is_available()),
            self.pneumatic.apu_bleed_air_valve(),
            self.fuel.left_inner_tank_has_fuel_remaining(),
        );

        self.electrical.update(
            context,
            electricity,
            &self.ext_pwr,
            &self.electrical_overhead,
            &self.emergency_electrical_overhead,
            &mut self.apu,
            &self.apu_overhead,
            &self.engine_fire_overhead,
            [&self.engine_1, &self.engine_2],
            &self.hydraulic,
            self.lgcius.lgciu1(),
        );

        self.electrical_overhead
            .update_after_electrical(&self.electrical, electricity);
        self.emergency_electrical_overhead
            .update_after_electrical(context, &self.electrical);
        self.payload.update(context);
        self.airframe
            .update(&self.fuel, &self.payload, &self.payload);
    }

    fn update_after_power_distribution(&mut self, context: &UpdateContext) {
        self.apu.update_after_power_distribution(
            &[&self.engine_1, &self.engine_2],
            [self.lgcius.lgciu1(), self.lgcius.lgciu2()],
        );
        self.apu_overhead.update_after_apu(&self.apu);

        self.asu.update();

        self.lgcius.update(
            context,
            &self.landing_gear,
            self.hydraulic.gear_system(),
            self.ext_pwr.output_potential().is_powered(),
        );

        self.radio_altimeters.update(context);

        self.hydraulic.update(
            context,
            &self.engine_1,
            &self.engine_2,
            &self.hydraulic_overhead,
            &self.autobrake_panel,
            &self.engine_fire_overhead,
            &self.lgcius,
            &self.emergency_electrical_overhead,
            &self.electrical,
            &self.pneumatic,
            &self.adirs,
        );

        self.reverse_thrust.update(
            context,
            [&self.engine_1, &self.engine_2],
            self.hydraulic.reversers_position(),
        );

        self.pneumatic.update_hydraulic_reservoir_spatial_volumes(
            self.hydraulic.green_reservoir(),
            self.hydraulic.blue_reservoir(),
            self.hydraulic.yellow_reservoir(),
        );

        self.hydraulic_overhead.update(&self.hydraulic);

        self.adirs.update(context, &self.adirs_overhead);
        self.adirs_overhead.update(context, &self.adirs);

        self.power_consumption.update(context);

        self.pneumatic.update(
            context,
            [&self.engine_1, &self.engine_2],
            &self.pneumatic_overhead,
            &self.engine_fire_overhead,
            &self.apu,
            &self.asu,
            &self.air_conditioning,
            [self.lgcius.lgciu1(), self.lgcius.lgciu2()],
        );
        self.air_conditioning
            .mix_packs_air_update(self.pneumatic.packs());
        self.air_conditioning.update(
            context,
            &self.adirs,
            [&self.engine_1, &self.engine_2],
            &self.engine_fire_overhead,
            &self.payload,
            &self.pneumatic,
            &self.pressurization_overhead,
            [self.lgcius.lgciu1(), self.lgcius.lgciu2()],
        );

        self.egpwc.update(&self.adirs, self.lgcius.lgciu1());
    }
}
impl SimulationElement for A330 {
    fn accept<T: SimulationElementVisitor>(&mut self, visitor: &mut T) {
        self.adirs.accept(visitor);
        self.adirs_overhead.accept(visitor);
        self.air_conditioning.accept(visitor);
        self.apu.accept(visitor);
        self.asu.accept(visitor);
        self.apu_fire_overhead.accept(visitor);
        self.apu_overhead.accept(visitor);
        self.payload.accept(visitor);
        self.airframe.accept(visitor);
        self.electrical_overhead.accept(visitor);
        self.emergency_electrical_overhead.accept(visitor);
        self.fuel.accept(visitor);
        self.pneumatic_overhead.accept(visitor);
        self.pressurization_overhead.accept(visitor);
        self.engine_1.accept(visitor);
        self.engine_2.accept(visitor);
        self.engine_fire_overhead.accept(visitor);
        self.electrical.accept(visitor);
        self.power_consumption.accept(visitor);
        self.ext_pwr.accept(visitor);
        self.lgcius.accept(visitor);
        self.radio_altimeters.accept(visitor);
        self.autobrake_panel.accept(visitor);
        self.hydraulic.accept(visitor);
        self.hydraulic_overhead.accept(visitor);
        self.landing_gear.accept(visitor);
        self.pneumatic.accept(visitor);
        self.egpwc.accept(visitor);
        self.reverse_thrust.accept(visitor);

        visitor.visit(self);
    }
}