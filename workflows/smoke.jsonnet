local reconstruction = import 'lib/reconstruction.libsonnet';

reconstruction({
  input_file: 'smoke_input.root',
  total_spills: 10,
  db_file: './smoke_geometry.db',
  field_file: './smoke_field.cvf',
  rntuple_file: 'smoke_reco_output.root',
  histo_file: 'smoke_reco_validation.root',
})
