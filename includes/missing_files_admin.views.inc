<?php
/**
 * Views module integration.
 */

/**
 * Implements hook_views_data_alter(&$data).
 */
function missing_files_admin_views_data_alter(&$data) {
  // file_exists
  $data['file_managed']['file_exists'] = array(
    'title' => t('Exists'),
    'help' => t('Whether or not the file exists in the file system.'),
    'field' => array(
      'handler' => 'views_handler_missing_files_admin_field_file_exists',
      'click sortable' => TRUE,
     ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
    'filter' => array(
      'handler' => 'views_handler_missing_files_admin_filter_file_exists',
    ),
  );
}

/**
 * Field handler to translate a file status into its readable form.
 */
class views_handler_missing_files_admin_field_file_exists extends views_handler_field {
  function render($values) {
    $value = $this->get_value($values);
    return _missing_files_admin_views_file_exists($value);
  }
}

/**
 * Adds and additional file status "Missing" for view filters.
 */
class views_handler_missing_files_admin_filter_file_exists extends views_handler_filter_in_operator {
  function get_value_options() {
    if (!isset($this->value_options)) {
      $this->value_options = _missing_files_admin_views_file_exists();
    }
  }
}

function _missing_files_admin_views_file_exists($choice = NULL) {
  $exists = array(
    0 => t('Missing'),
    1 => t('Exists'),
  );

  if (isset($choice)) {
    return isset($exists[$choice]) ? $exists[$choice] : t('Unknown');
  }

  return $exists;
}

