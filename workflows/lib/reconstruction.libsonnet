// Shared body of the reconstruction workflows (smoke, tracking,
// run_reconstruction): one source per input kind, the spectrometer tracking
// pipeline, optionally the per-subdetector placeholder recos, and the
// output/validation module. Per-workflow files only set the parameters.
function(params)
  local p = {
    simulation: true,
    // Also read and write out sim_particles (only with simulation). Off by
    // default: aegir keeps every Geant4 particle, far more than all the hits.
    sim_particles: false,
    // Process only the first max_spills time windows ("spill" data cells);
    // null processes every window the input spans.
    max_spills: null,
    // Sliding time windows: width, step between window starts, and how far
    // out of time order (behind the latest hit read) an input hit may be.
    window_ns: 3000,
    step_ns: 1000,
    max_lag_ns: 1000,
    // Path to shannon's digitised-hit output (or a make_test_input file).
    input_file: error 'input_file is required',
    db_file: '../geometry/my_ship_geometry.db',
    // Spectrometer field map in the SHiP field-map format: the `file`, the
    // `map` inside it, and an optional `translation` [x, y, z] in mm placing
    // the map's origin in the global frame. The default puts it at the
    // magnet centre, as FairShip and aegir do.
    field: {
      file: 'spectrometer_dipole.root',
      map: 'spectrometer_dipole',
      translation: [0, 0, 89570],
    },
    rntuple_file: error 'rntuple_file is required',
    histo_file: null,
    // Include the per-subdetector placeholder reconstruction modules.
    full_reco: true,
  } + params;
  {
    stage: 'reconstruction',
    driver: {
      // One "spill" data cell per time window of the input, counted from the
      // file itself. Must see the same settings as rntuple_source below.
      cpp: 'time_window_driver',
      input_file: p.input_file,
      layer: 'spill',
      simulation: p.simulation,
      sim_particles: p.sim_particles,
      window_ns: p.window_ns,
      step_ns: p.step_ns,
    } + (if p.max_spills != null then { max_spills: p.max_spills } else {}),

    sources: {
      rntuple_source: {
        cpp: 'read_digitised_hits',
        input_file: p.input_file,
        layer: 'spill',
        simulation: p.simulation,
        sim_particles: p.sim_particles,
        window_ns: p.window_ns,
        step_ns: p.step_ns,
        max_lag_ns: p.max_lag_ns,
      },
      tracking_geometry: {
        cpp: 'acts_geometry_provider',
        db_file: p.db_file,
      },
      spectrometer_field: { cpp: 'acts_field_provider' } + p.field,
    },

    modules: {
      spectrometer_tracking: {
        cpp: 'spectrometer_tracking',
        layer: 'spill',
      },
      rec_output: {
        cpp: 'rec_output_module',
        rntuple_file: p.rntuple_file,
        simulation: p.simulation,
        sim_particles: p.sim_particles,
      } + (if p.histo_file != null then { histo_file: p.histo_file } else {}),
    } + (
      if p.full_reco then {
        upstream_tagger_reco: { cpp: 'upstream_tagger_reco', layer: 'spill' },
        surround_tagger_reco: { cpp: 'surround_tagger_reco', layer: 'spill' },
        calorimeter_reco: { cpp: 'calorimeter_reco', layer: 'spill' },
        timing_detector_reco: { cpp: 'timing_detector_reco', layer: 'spill' },
      } else {}
    ),
  }
