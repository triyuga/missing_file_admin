<?php
/**
 * Additional code for missing_files_admin
 */


/**
 * Verify all files in the files table and check their representation on the file system.
 * Therefore we create a batch job and separate the whole set into separate junks.
 * @param $process
 *   boolean whether to process the batch immediately, not needed of being set as
 *   on form submission a batch will be triggered automatically
 */
function missing_files_admin_process_verification($process = TRUE) {
  // calculate number of files
  $total = missing_files_admin_get_count();
  // split processing into junks
  $batch_size = variable_get('missing_files_admin_process_batch_size', 100);
  $operations = array();
  for ($i = 0; $i < $total; $i += $batch_size) {
    // the last op will be smaller, don't exceed total size
    $count = ($total > ($i + $batch_size)) ? $batch_size : ($total - $i);
    $operations[] = array('missing_files_admin_verify_range', array($i, $count, $total));
  }
  // Set batch processor
  $batch = array(
    'operations' => $operations,
    'finished' => 'missing_files_admin_verify_finish', // called after operations have been finished
    'title' => t('Identify and flag missing physical files'),
    'init_message' => t('File Checker is starting to process %total files', array('%total' => $total)),
    'progress_message' => NULL, // suppress here, message to be set in operation function
    'error_message' => t('File Checker has encountered an error.'),
    'progressive' => FALSE,
  );
  batch_set($batch); // no need to call
  if ($process) {
    batch_process();
  }
}

/**
 * This batch operation callback checks a range of files.
 * @param $from
 *   file offset
 * @param $count
 *   number of files to check
 * @param $total
 *   total number of files
 * @param $context
 *   batch callback context information
 */
function missing_files_admin_verify_range($from, $count, $total, &$context) {

  $result = db_select('file_managed', 'f')
    ->fields('f')
    ->range($from, $count)
    ->execute();
  // dvm('checking records ' . ($from+1) . '-'.($from+$count). '. Found ' . count($result) . ' records.');

  foreach ($result as $data) {
    // Temporary files (those marked with status = 0) older than the maximum temp age
    // were, or will be, removed during a cron run.
    $files = db_select('file_managed', 'f')
      ->fields('f')
      ->condition('fid', $data->fid)
      ->condition('status', 0, '>') // Skip temporary files.
      ->execute();
    foreach ($files as $file) {
      missing_files_admin_verify($file);
      break; // only need to check one, more would be redundant
    }
  }
  // correct status message here, as we process in larger junks
  $context['message'] = t('Processed @current out of @total.', array('@current' => ($from + $count), '@total' => $total));
}

/**
 * Check if a file contained in the files table physically exists on the file system.
 * If a file cannot be accessed, we flag the file as missed, i. e. FILE_STATUS_MISSING.
 * @param $file
 *   file object
 */
function missing_files_admin_verify($file) {
  // determine file path
  $file_public_path = variable_get('file_public_path', NULL);
  $dir = DRUPAL_ROOT . '/' . $file_public_path . '/';
  $path  = str_replace('public://', $dir, $file->uri);

  // check if file can be accessed
  if (!file_exists($path) || !is_readable($path) || !is_writable($path)) {
    missing_files_admin_set_status($file->fid, FALSE);
    // dvm('file_exists = 0, ' . $path);
  }
  else {
    missing_files_admin_set_status($file->fid, TRUE);
    // dvm('file_exists = 1, ' . $path);
  }
}

function missing_files_admin_set_status($fid, $exists = TRUE) {
  $exists = $exists ? 1 : 0;
  $fid = (int) $fid;

  db_update('file_managed')
    ->fields(array('file_exists' => $exists))
    ->condition('fid', $fid)
    ->execute();
}

/**
 * Implements hook_cron()
 *
 * Process the file system checking on cron if demanded. This should only be used on small sites if at all.
 */
function missing_files_admin_cron() {
  if (variable_get('missing_files_admin_process_on_cron', FALSE)) {
    missing_files_admin_process_verification(TRUE);
  }
}

/**
 * Batch callback finisher operation
 * @param $success
 *   boolean about operation success
 * @param $results
 *   potential ouput
 * @param $operations
 *   operations processed
 */
function missing_files_admin_verify_finish($success, $results, $operations) {
  if ($success) {
    variable_set('missing_files_admin_last_run', REQUEST_TIME);
    drupal_set_message(t('File verification has been completed.'));
    watchdog('missing_files_admin', t('File verification has been completed.'));
  }
  else {
    // A fatal error occurred during batch processing
    $error_operation = reset($operations);
    $operation = array_shift($error_operation);
    $arguments = array_shift($error_operation);
    $arguments_as_string = implode(', ', $arguments);
    watchdog('missing_files_admin', "Error on file verification when calling operation '%s'('%s')", array($operation, $arguments_as_string), WATCHDOG_ERROR);
    drupal_set_message(t('An error occurred and has been recorded in the system log.'), 'error');
  }
}

/**
 * Total count of files.
 * @return
 *   SQL statement string
 */
function missing_files_admin_get_count() {
  $total = db_query('SELECT count(*) FROM {file_managed}')->fetchField();
  return $total;
}

/**
 * Get a list of (relevant) db columuns in the files table. We could also call drupal_get_schema('files'),
 * but this is more efficient and we have the columns in the order we want them to have.
 *
 * @return
 *  array of file table columns
 */
function missing_files_admin_get_db_table_file_cols() {
  return array(
    'fid' => array('type' => 'serial'),
    'timestamp' => array('type' => 'int'),
    'filename' => array('type' => 'varchar'),
    'uri' => array('type' => 'varchar'),
    'filemime' => array('type' => 'varchar'),
    'filesize' => array('type' => 'int'),
    'uid' => array('type' => 'int'),
    'status' => array('type' => 'int'),
    'type' => array('type' => 'varchar'),
  );
}
