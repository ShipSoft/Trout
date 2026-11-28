local reconstruction = import 'lib/reconstruction.libsonnet';

// smoke.jsonnet, but reading the input indexed rather than streamed: with no
// lag allowed, any RNTuple at all out of time order is indexed. scripts/smoke.sh
// requires the two to reconstruct the same tracks.
reconstruction({
  input_file: 'smoke_input.root',
  sim_particles: true,
  max_lag_ns: 0,
  db_file: './smoke_geometry.db',
  field: { file: './smoke_field.root', map: 'smoke_field' },
  rntuple_file: 'smoke_indexed_output.root',
  histo_file: 'smoke_indexed_validation.root',
})
