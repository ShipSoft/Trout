// Shared body of the reconstruction workflows (smoke, tracking,
// run_reconstruction): one source per input kind, the spectrometer tracking
// pipeline, optionally the per-subdetector placeholder recos, and the
// output/validation module. Per-workflow files only set the parameters.
function(params)
  local p = {
    simulation: true,
    total_spills: 1,
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
      cpp: 'generate_layers',
      layers: {
        spill: { parent: 'job', total: p.total_spills },
      },
    },

    sources: {
      rntuple_source: {
        cpp: 'read_digitised_hits',
        input_file: p.input_file,
        layer: 'spill',
        simulation: p.simulation,
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
