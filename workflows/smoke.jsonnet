local reconstruction = import 'lib/reconstruction.libsonnet';

reconstruction({
  input_file: 'smoke_input.root',
  // The smoke input is tiny, so also cover the sim_particles path.
  sim_particles: true,
  db_file: './smoke_geometry.db',
  field: { placements: './smoke_placements.json' },
  rntuple_file: 'smoke_reco_output.root',
  histo_file: 'smoke_reco_validation.root',
})
