local reconstruction = import 'lib/reconstruction.libsonnet';

reconstruction({
  input_file: '../shannon/digitised_hits_time.root',
  rntuple_file: 'track_fit_results.root',
  full_reco: false,
})
