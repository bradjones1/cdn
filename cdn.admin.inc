<?php

/**
 * @file
 * Settings administration UI.
 */


//----------------------------------------------------------------------------
// Form API callbacks.

/**
 * Form definition; general settings.
 */
function cdn_admin_general_settings_form($form, &$form_state) {
  $form = array();
  _cdn_settings_form_prepare($form, $form_state);


  return system_settings_form($form);
}

/**
 * Form definition; details.
 */
function cdn_admin_details_form($form, &$form_state) {
  $form = array();
  _cdn_settings_form_prepare($form, $form_state);

  return system_settings_form($form);
}


//----------------------------------------------------------------------------
// Private functions.

/**
 * Helper function to check if the requirements of the CDN module have been
 * met. If any requirement errors exist, they are aggregated into a single
 * error message and are subsequently displayed.
 *
 * @return
 *   The number of requirement errors.
 */
function _cdn_admin_check_requirements() {
  // Check run-time requirements of the CDN integration module.
  module_load_install('cdn');
  $requirements = cdn_requirements('runtime');
  $problematic_statuses = array(REQUIREMENT_WARNING, REQUIREMENT_ERROR);

  // Filter out the requirement errors and display these, with links back to
  // the admin/reports/status page.
  $errors = array();
  foreach ($requirements as $requirement => $details) {
    if (in_array($details['severity'], $problematic_statuses)) {
      $errors[] = $details['description'];
    }
  }
  if (!empty($errors)) {
    drupal_set_message(
      t(
          'The CDN module has detected the following <em>potential</em>
          problems in its configuration:<br />
          !error-list
          You can also see them on the !status-report.',
          array(
            '!status-report' => l(t('status report'), 'admin/reports/status'),
            '!error-list' => theme('item_list', array('items' => $errors)),
          )
      ),
      'error'
    );
  }

  return count($errors);
}

function _cdn_settings_form_prepare(&$form, $form_state) {
  // Do some checks, but prevent them from showing up twice.
  if (empty($form_state['input'])) {
    _cdn_admin_check_requirements();

    // Advanced help.
    _cdn_check_advanced_help();
    $form['#attributes']['class'] = 'cdn-settings';
    $form['#attached']['css'][] = drupal_get_path('module', 'cdn') . '/cdn.admin.css';
  }
}

function _cdn_check_advanced_help() {
  if (!module_exists('advanced_help')) {
    $sql = "SELECT filename
            FROM {system}
            WHERE type = 'module'
            AND name = 'advanced_help'";
    $filename = db_query($sql)->fetchField();
    if ($filename && file_exists($filename)) {
      drupal_set_message(t(
        'If you enable the <a href="@modules-url">Advanced Help</a> module,
        the CDN module will provide more and better help.',
        array('@modules-url' => url('admin/modules'))
      ));
    }
    else {
      drupal_set_message(t(
        'If you install the <a href="@adv-help-url">Advanced Help</a> module,
        the CDN module will provide more and better help.',
        array('@adv-help-url' => url('http://drupal.org/project/advanced_help'))
      ));
    }
  }
}

function _cdn_help($topic) {
  if (!module_exists('advanced_help')) {
    return '';
  }
  else {
    return theme(
      'advanced_help_topic',
      array('module' => 'cdn', 'topic' => $topic)
    );
  }
}
