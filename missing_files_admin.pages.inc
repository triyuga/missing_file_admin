<?php
/**
 * Missing files admin pages.
 */

/**
 * @file missing_files_admin.pages.inc
 * Lists and configuration pages for the file checker.
 */

/**
 * Configuration options form of the file checker.
 * @return
 *   form array
 */
function missing_files_admin_admin_settings($form, $form_state) {

  $form['missing_files_admin_process_on_cron'] = array(
    '#type' => 'radios',
    '#title' => t('Process on cron'),
    '#default_value' => variable_get('missing_files_admin_process_on_cron', 0),
    '#options' => array(
      0 => t('Disabled'),
      1 => t('Enabled'),
    ),
    '#description' => t('Do automatic file checking on each cron run. Enable this option only on small sites with a few files.<br/>You can process the file checking manually below or run the drush command "drush file-checker-run"'),
  );

  $form['missing_files_admin_process_batch_size'] = array(
    '#type' => 'textfield',
    '#title' => t('Process batch size'),
    '#default_value' => variable_get('missing_files_admin_process_batch_size', 10000),
    '#description' => t('Process file checking in junks instead of one by one preventing timeouts, memory outages etc.<br/>A higher values usually results in better performance whereas a lower value increases percentage bar accuracy.'),
  );

  $form['manual'] = array(
    '#type' => 'fieldset',
    '#title' => t('Process manually'),
  );

  $total = db_query('SELECT count(*) FROM {file_usage}')->fetchField();
  $total_files = t('There are currently @total files.', array('@total' => $total));
  if (!variable_get('missing_files_admin_last_run', 0)) {
    $form['missing_files_admin_process_manually']['#value'] = $total_files . ' ' . t('File verification has never been run.');
  }
  else {
    $form['missing_files_admin_process_manually']['#value'] = $total_files . ' ' . t('File verification has been last run on @time', array('@time' => format_date(variable_get('missing_files_admin_last_run', 0), 'short')));
  }

  $form['manual']['manual_run'] = array(
    '#type' => 'submit',
    '#name' => 'manual_run',
    '#value' => t('Flag missing files'),
    '#suffix' => '<br/><br/>',
  );

  $form['#submit'][] = 'missing_files_admin_admin_settings_submit';

  return system_settings_form($form);
}

/**
 * Submit handler for admin settings form.
 * @param $form
 *   form array
 * @param $form_state
 *   form values
 */
function missing_files_admin_admin_settings_submit($form, $form_state) {
  // Run file checking manually.
  if ($form_state['clicked_button']['#name'] == 'manual_run') {
    missing_files_admin_process_verification(FALSE); // batch_process is triggered automatically
  }
}

/**
 * File overview page list all files including some filters and sorting.
 * See dblog.admin.inc dblog_overview() for an example.
 */
function missing_files_admin_overview() {
  $rows = array();

  // filter form
  $build['missing_files_admin_filter_form'] = drupal_get_form('missing_files_admin_filter_form');

  // define table headers here
  $header = array(
    array(
      'data' => t('File ID'),
      'field' => 'f.fid',
    ),
    array(
      'data' => t('File name'),
      'field' => 'f.filename',
    ),
    array(
      'data' => t('File path'),
      'field' => 'f.uri',
    ),
    array(
      'data' => t('File Mime'),
      'field' => 'f.filemime',
    ),
    array(
      'data' => t('File size'),
      'field' => 'f.filesize',
    ),
    array(
      'data' => t('Timestamp'),
      'field' => 'f.timestamp',
      'sort' => 'desc',
    ),
    array(
      'data' => t('User'),
      'field' => 'f.uid',
    ),
    array(
      'data' => t('Status'),
      'field' => 'f.status',
    ),
    array(
      'data' => t('Type'),
      'field' => 'f.type',
    ),
    array(
      'data' => t('Where Used'),
      'field' => 'u.type'.'u.id',
    ),
  );
  $select = db_select('file_managed', 'f')
    ->extend('TableSort')
    ->extend('PagerDefault');
  $select->join('file_usage', 'u', 'f.fid = u.fid');
  $select->addField('u', 'type', 'used_type');
  $select->addField('u', 'id', 'used_type_id');
  $select->fields('f');
  $results = $select;

  // apply filters
  $filters = $build['missing_files_admin_filter_form']['filters'];
  if (!empty($filters['filter_filename']['#value'])) {
    $results = $results
      ->condition('f.status', $filters['filter_filename']['#value'], 'LIKE');
  }
  if (!empty($filters['filter_filemime']['#value'])) {
    $results = $results
      ->condition('f.filemime', $filters['filter_filemime']['#value']);
  }
  if (isset($_SESSION['missing_files_admin_overview_filter_status'])) {
    $results = $results
      ->condition('f.status', $_SESSION['missing_files_admin_overview_filter_status'], '>=');
  }
  if (!empty($filters['filter_uid']['#value'])) {
    $results = $results
      ->condition('f.uid', $filters['filter_uid']['#value'], 'IN');
  }

  $results = $results
    ->limit(100)
    ->orderByHeader($header)
    ->execute();
  foreach ($results as $file) {
    $used_path = $file->used_type .'/'. $file->used_type_id;
    $rows[] = array(
      'data' => array(
        $file->fid,
        $file->filename,
        $file->uri,
        $file->filemime,
        $file->filesize,
        format_date($file->timestamp, 'short'),
        l(user_load($file->uid)->name, 'user/' . $file->uid),
        $file->status,
        $file->type,
        l($used_path, $used_path),
      ),
    );
  }

  $build['missing_files_admin_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No files available.'),
  );
  $build['missing_files_admin_pager'] = array('#theme' => 'pager');

  return $build;
}

/**
 * Filter form for overview page.
 * TODO: Use advanced filtering as in user_filter_form() and theme_user_filters().
 *
 * @param $form_state
 *   array of form values
 * @return
 *   form array
 */
function missing_files_admin_filter_form($form, $form_state) {

  $filter_filename =
    isset($_SESSION['missing_files_admin_overview_filter_filename']) ?
    $_SESSION['missing_files_admin_overview_filter_filename'] : '';
  $filter_filemime =
    isset($_SESSION['missing_files_admin_overview_filter_filemime']) ?
    $_SESSION['missing_files_admin_overview_filter_filemime'] : '';
  $filter_uid =
    isset($_SESSION['missing_files_admin_overview_filter_uid']) ?
    $_SESSION['missing_files_admin_overview_filter_uid'] : NULL;
  $filter_status =
    isset($_SESSION['missing_files_admin_overview_filter_status']) ?
    $_SESSION['missing_files_admin_overview_filter_status'] : 2;

  $expand = !empty($filter_filename) || !empty($filter_filemime) || !empty($filter_uid) || $filter_status != 2;

  $form['filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('Show only items where'),
    '#collapsible' => TRUE,
    '#collapsed' => !$expand,
  );

  $form['filters']['filter_filename'] = array(
    '#type' => 'textfield',
    '#title' => t('File name'),
    '#default_value' => $filter_filename,
  );

  $names[''] = t('All mime types');
  foreach (_missing_files_admin_get_file_mime_types() as $type) {
    $names[$type] = t('!type', array('!type' => t($type)));
  }
  $form['filters']['filter_filemime'] = array(
    '#type' => 'select',
    '#title' => t('File Mime type'),
    '#options' => $names,
    '#default_value' => $filter_filemime,
  );
  $form['filters']['filter_uid'] = array(
    '#type' => 'textfield',
    '#title' => t('User Id'),
    '#default_value' => $filter_uid,
  );
  $form['filters']['filter_status'] = array(
    '#type' => 'select',
    '#title' => t('File status'),
    '#options' => array(
      '' => t('Any'),
      0 => t('Temporary'),
      1 => t('Permanent'),
      2 => t('Missing'),
    ),
    '#default_value' => $filter_status,
  );

  $form['filters']['actions'] = array(
    '#type' => 'actions',
    '#attributes' => array('class' => array('container-inline')),
  );
  $form['filters']['actions'] = array(
    '#type' => 'submit',
    '#value' => t('Filter'),
  );
  $form['filters']['actions']['reset'] = array(
    '#type' => 'submit',
    '#value' => t('Reset'),
  );

  return $form;
}

/**
 * Submit handler for filter form.
 * @param $form
 *   form array
 * @param $form_state
 *   form values
 */
function missing_files_admin_filter_form_submit($form, &$form_state) {
  $op = $form_state['values']['op'];
  switch ($op) {
    case t('Reset'): // reset form
      foreach ($form_state['values'] as $form_value_id => $form_value) {
        if (strpos($form_value_id, 'filter_') === 0) {
          unset($_SESSION['missing_files_admin_overview_$form_value_id']);
        }
      }
      break;
    case t('Filter'): // filter form
      foreach ($form_state['values'] as $form_value_id => $form_value) {
        if (strpos($form_value_id, 'filter_') === 0) {
          if ($form_value) {
            $_SESSION['missing_files_admin_overview_'.$form_value_id] = $form_value;
          }
          else {
            unset($_SESSION['missing_files_admin_overview_'.$form_value_id]);
          }
        }
      }
      break;
  }
  return 'admin/reports/missing_files_admin';
}

/**
 * Get a list of file mime types.
 *
 * @return
 *   array of file mime types
 */
function _missing_files_admin_get_file_mime_types() {
  $types = array();
  $result = db_query('SELECT DISTINCT(filemime) FROM {file_managed} ORDER BY filemime');
  foreach ($result as $object) {
    $types[] = $object->filemime;
  }
  return $types;
}






function missing_files_admin_update_usage_form($form, $form_state) {
  $form = array();

  $fields = array();
  $results = db_select('field_config', 'fc')
    ->fields('fc', array('field_name', 'data'))
    ->condition('type', array('text_long', 'text_with_summary'), 'IN')
    ->execute()
    ->fetchAllAssoc('field_name');
  foreach ($results as $field_name => $result) {
    $fields[$field_name] = $field_name;
  }

  $form['fields'] = array(
    '#type' => 'checkboxes',
    '#title' => 'Fields to search',
    '#options' => $fields,
    '#default_value' => $fields,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Update File Usage Records',
  );

  return $form;
}

function missing_files_admin_update_usage_form_submit($form, $form_state) {

  $file_managed = db_select('file_managed', 'fm')
    ->fields('fm')
    ->execute()
    ->fetchAllAssoc('fid');

  $file_usage = db_select('file_usage', 'fu')
    ->fields('fu')
    ->execute()
    ->fetchAllAssoc('fid');

  $fields = array();
  foreach ($form_state['values']['fields'] as $key => $checked) {
    if ($checked) {
      $fields[] = $key;
    }
  }

  $new_references_count = 0;
  foreach ($file_managed as $file) {
    foreach ($fields as $field) {
      $references = array();
      $table = "field_data_{$field}";

      // Search 1
      $like = $file->filename;
      $results = db_select($table, 'f')
      ->fields('f', array('entity_type', 'entity_id'))
      ->condition("{$field}_value", '%' . db_like($like) . '%', 'LIKE')
      ->execute()
      ->fetchAllAssoc('entity_id');
      foreach ($results as $result) {
        $references[$file->fid][] = $result;
      }

      // Search 2
      $like = '"fid":"'. $file->fid .'"';
      $results = db_select($table, 'f')
      ->fields('f', array('entity_type', 'entity_id'))
      ->condition("{$field}_value", '%' . db_like($like) . '%', 'LIKE')
      ->execute()
      ->fetchAllAssoc('entity_id');
      foreach ($results as $result) {
        $references[$file->fid] = $result;
      }

      foreach ($references as $reference) {
        $record_exists = FALSE;
        if (in_array($file->fid, array_keys($file_usage)) && $file_usage[$file->fid]->id == $reference->entity_id) {
          // $file_usage record already exists.
          continue;
        }
        else {
          // Create file usage record.
          $row = array(
            'fid' => $file->fid,
            'module' => $reference->entity_type,
            'type' => $reference->entity_type,
            'id' => $reference->entity_id,
            'count' => 1,
          );
          db_insert('file_usage')
            ->fields($row)
            ->execute();

          $new_references_count++;
        }
      }
    }
  }
  drupal_set_message("{$new_references_count} new file usage records were added.");
}
