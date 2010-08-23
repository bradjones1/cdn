<?php
// $Id$

/**
 * @file
 * Settings administration UI.
 */


//----------------------------------------------------------------------------
// Menu system callbacks.

/**
 * Menu callback; mode-specific settings form (automatically load the correct
 * form).
 */
function cdn_admin_mode_settings_form_router() {
  $mode = variable_get(CDN_MODE_VARIABLE, CDN_MODE_BASIC);
  $form_function = ($mode == CDN_MODE_BASIC) ? 'cdn_admin_basic_settings_form' : 'cdn_admin_advanced_settings_form';
  return drupal_get_form($form_function);
}


//----------------------------------------------------------------------------
// Form API callbacks.

/**
 * Form definition; general settings.
 */
function cdn_admin_general_settings_form(&$form_state) {
  ctools_include('dependent');

  // Prevent requirement errors from showing up twice.
  if (empty($form_state['post'])) {
    _cdn_admin_check_requirements();
  }

  $form['#submit'] = array('cdn_admin_settings_submit_show_cache_warning');

  $form[CDN_STATUS_VARIABLE] = array(
    '#type' => 'radios',
    '#title' => t('Status'),
    '#description' => t(
      "If you don't want to use the CDN to serve files to your visitors yet,
      but you do want to see if it's working well for your site, then enable
      testing mode.<br />It will only serve files from the CDN if they have
      the %cdn-testing-mode-permission permission.",
      array('%cdn-testing-mode-permission' => 'access files on CDN when in testing mode')
    ),
    '#required' => TRUE,
    '#options' => array(
      CDN_DISABLED => t('Disabled'),
      CDN_TESTING  => t('Testing mode'),
      CDN_ENABLED  => t('Enabled'),
    ),
    '#default_value' => variable_get(CDN_STATUS_VARIABLE, CDN_DISABLED),
  );

  $form[CDN_MODE_VARIABLE] = array(
    '#type' => 'radios',
    '#title' => t('Mode'),
    '#description' => t(
      "<strong>Basic mode</strong> will simply prepend another URL to the
      complete file URL. Therefor, it only works for CDNs that support the
      Origin Pull technique.<br />
      <strong>Advanced mode</strong> uses the daemon to synchronize files and
      can be used with both Origin Pull and Push CDNs. If you've got an Origin
      Pull CDN and want to process files before they're synced to the CDN, it
      is also recommended to use this mode."
    ),
    '#required' => TRUE,
    '#options'  => array(
      CDN_MODE_BASIC    => t('Basic'),
      CDN_MODE_ADVANCED => t('Advanced'),
    ),
    '#default_value' => variable_get(CDN_MODE_VARIABLE, CDN_MODE_BASIC),
  );

  $form[CDN_STATS_VARIABLE] = array(
    '#type' => 'checkbox',
    '#title' => t('Display statistics'),
    '#description' => t(
      'Shows the CDN integration statistics of the current page, to users with
      the %access-per-page-statistics permission.',
      array('%access-per-page-statistics' => 'access per-page statistics')
    ),
    '#return_value'  => CDN_ENABLED,
    '#default_value' => variable_get(CDN_STATS_VARIABLE, CDN_DISABLED),
    '#process'       => array('ctools_dependent_process'),
    '#dependency'    => array('radio:' . CDN_STATUS_VARIABLE => array(CDN_TESTING, CDN_ENABLED)),
  );

  return system_settings_form($form);
}

/**
 * Form definition; basic settings.
 */
function cdn_admin_basic_settings_form(&$form_state) {
  // Prevent requirement errors from showing up twice.
  if (empty($form_state['post'])) {
    _cdn_admin_check_requirements();
  }

  $form['#submit'] = array('cdn_admin_settings_submit_show_cache_warning');

  $form[CDN_BASIC_URL_VARIABLE] = array(
    '#type'          => 'textfield',
    '#title'         => t('CDN URL'),
    '#description'   => t("The CDN URL prefix that should be used (do not
                           include a trailing slash). Only works for CDNs that
                           support Origin Pull.<br />
                           <strong>WARNING</strong>: do not use subdirectories
                           when you're serving CSS files from the CDN. The
                           references to images and fonts from within the CSS
                           files will break because the URLs are no longer
                           valid."),
    '#required'      => TRUE,
    '#size'          => 35,
    '#default_value' => variable_get(CDN_BASIC_URL_VARIABLE, ''),
  );

  $form[CDN_BASIC_EXTENSIONS_VARIABLE] = array(
    '#type'          => 'textfield',
    '#title'         => t('Allowed extensions'),
    '#description'   => t('Only files with these extensions will be served from the CDN.'),
    '#required'      => TRUE,
    '#default_value' => variable_get(CDN_BASIC_EXTENSIONS_VARIABLE, CDN_BASIC_EXTENSIONS_DEFAULT),
  );

  $form[CDN_BASIC_HTTPS_VARIABLE] = array(
    '#type'          => 'checkbox',
    '#title'         => t('CDN supports HTTPS'),
    '#description'   => t('Check this if your CDN supports HTTPS (on the same
                           host) and you would like to serve CDN content via
                           HTTPS if your page is being served via HTTPS.'),
    '#default_value' => variable_get(CDN_BASIC_HTTPS_VARIABLE, FALSE),
  );

  return system_settings_form($form);
}

/**
 * Form definition; advanced settings.
 */
function cdn_admin_advanced_settings_form(&$form_state) {
  // Prevent requirement errors from showing up twice.
  if (empty($form_state['post'])) {
    _cdn_admin_check_requirements();
  }

  $form['#submit'] = array('cdn_admin_settings_submit_show_cache_warning');

  // Immediately show the user the current status, unless the user is
  // currently editing the values in the form.
  if (!isset($_POST['form_build_id'])) {
    $synced_files_db = variable_get(CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE, FALSE);
    if ($synced_files_db !== FALSE) {
      if (file_exists($synced_files_db) && @fopen($synced_files_db, 'r')) {
        drupal_set_message(t('The synced files database was found and can be opened for reading.'));
      }
      else {
        drupal_set_message(t('The synced files database could not be opened for reading!', 'error'));
      }
    }
  }

  $form[CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE] = array(
    '#type'          => 'textfield',
    '#title'         => t('Synced files database'),
    '#description'   => t('Enter the full path to the daemon\'s synced files database file.'),
    '#required'      => TRUE,
    '#size'          => 60,
    '#default_value' => variable_get(CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE, ''),
  );

  $form[CDN_ADVANCED_HTTPS_VARIABLE] = array(
    '#type'          => 'checkbox',
    '#title'         => t('CDN supports HTTPS'),
    '#description'   => t('Check this if your CDN supports HTTPS (on the same
                           host) and you would like to serve CDN content via
                           HTTPS if your page is being served via HTTPS.'),
    '#default_value' => variable_get(CDN_ADVANCED_HTTPS_VARIABLE, FALSE),
  );

  return system_settings_form($form);
}

/**
 * Form definition; other settings.
 */
function cdn_admin_other_settings_form(&$form_state) {
  // Prevent requirement errors from showing up twice.
  if (empty($form_state['post'])) {
    _cdn_admin_check_requirements();
  }

  $form['#submit'] = array('cdn_admin_settings_submit_show_cache_warning');

  $form[CDN_DRUPAL_ROOT_VARIABLE] = array(
    '#type' => 'textfield',
    '#title' => t('Drupal root directory'),
    '#description' => t(
      'In 99% of the cases the default value is sufficient and correct. In
      some advanced setups that make use of symbolic links however, it is
      possible that the Drupal root directory is incorrectly detected. In
      those cases, you should enter it here.<br />
      <strong>This setting only affects the status report, it does not affect
      the CDN integration itself in any way.</strong>'
    ),
    '#required'      => TRUE,
    '#default_value' => variable_get(CDN_DRUPAL_ROOT_VARIABLE, realpath('.')),
  );

  $form[CDN_EXCLUSION_VARIABLE] = array(
    '#type' => 'textarea',
    '#title' => t('Paths excluded from the CDN'),
    '#default_value' => variable_get(CDN_EXCLUSION_VARIABLE, CDN_EXCLUSION_DEFAULT),
    '#description' => t(
      "Enter one page per line as Drupal paths. The '*' character is a
      wildcard. Example paths are %blog for the blog page and %blog-wildcard
      for every personal blog. %front is the front page.<br />
      <strong>JavaScript files are excluded by default. This is necessary to
      ensure a painless out-of-the-box experience.</strong><br />
      <strong><u>Full explanation:</u></strong> it's necessary prevent any
      possible cross-domain AJAX requests, which would violate the <a
      href=\"http://en.wikipedia.org/wiki/Same_origin_policy\">same origin
      policy</a> of browsers. Such violations potentially result in broken
      functionality. Note that <u>even requests to subdomains such as
      cdn.yourdomain.com count as cross-domain requests!</u><br />
      You can opt-in to including JavaScript files by default and then exclude
      problematic ones, but then you should carefully note which JavaScript
      files perform AJAX requests. You can prevent all potential problems by
      using <a href=\"http://en.wikipedia.org/wiki/JSON#JSONP\">JSONP</a>,
      which is a work-around to allow for cross-domain AJAX
      requests.</strong>",
      array(
        '%blog'          => 'blog',
        '%blog-wildcard' => 'blog/*',
        '%front'         => '<front>'
      )
    ),
  );

  $form[CDN_EXCLUSION_LOGGED_IN_VARIABLE] = array(
    '#type'          => 'textarea',
    '#title'         => t('Paths excluded from the CDN if logged in'),
    '#default_value' => variable_get(CDN_EXCLUSION_LOGGED_IN_VARIABLE, CDN_EXCLUSION_LOGGED_IN_DEFAULT),
    '#description'   => t("Enter one Drupal path per line. Same rules as above apply here."),
  );

  return system_settings_form($form);
}

/**
 * Default validate callback for the general settings form.
 */
function cdn_admin_general_settings_form_validate($form, &$form_state) {
  // Validate the synced files DB whenever advanced mode is enabled. It must
  // be configured (on the advanced settings form) *before* advanced mode can
  // be enabled!
  $enabled  = isset($form_state['values'][CDN_STATUS_VARIABLE]) && $form_state['values'][CDN_STATUS_VARIABLE] != CDN_DISABLED;
  $advanced = isset($form_state['values'][CDN_MODE_VARIABLE])   && $form_state['values'][CDN_MODE_VARIABLE]   == CDN_MODE_ADVANCED;
  if ($enabled && $advanced) {
    $synced_files_db = variable_get(CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE, '');
    if (!_cdn_admin_validate_synced_files_db($synced_files_db, CDN_STATUS_VARIABLE)) {
      drupal_set_message(t('Please correct the above error in the <em>Advanced mode</em> settings form first.'), 'error');
    }
  }
}

/**
 * Default validate callback for the advanced settings form.
 */
function cdn_admin_advanced_settings_form_validate($form, &$form_state) {
  // Validate the synced files DB when it's filled out on the advanced
  // settings form.
  if (isset($form_state['values'][CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE])) {
    $synced_files_db = $form_state['values'][CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE];
    _cdn_admin_validate_synced_files_db($synced_files_db, CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE);
  }
}

/**
 * Submit callback for all settings forms, that shows a warning whenever
 * settings have been changed: the caches might need to be cleared.
 */
function cdn_admin_settings_submit_show_cache_warning($form, &$form_state) {
  drupal_set_message(t('You have just changed some settings. This might affect
                        the file URLs that have been cached in nodes, views,
                        or elsewhere.<br />
                        You now may want to clear the cached data, to ensure
                        the new CDN file URLs are used immediately. This can
                        be done in the <a href="!link">"Clear cached data"
                        field set</a> in the "Performance" settings form.',
                        array(
                          '!link' => url('admin/settings/performance', array('fragment' => 'clear-cache'))
                        )
                      ),
                      'info'
  );
}


//----------------------------------------------------------------------------
// Private functions.

/**
 * Helper function to validate a possible synced files DB value.
 *
 * @param $synced_files_db
 *   A user-entered synced files DB value.
 * @param $name
 *   The name of the form item on which to set errors, if any.
 * @return
 *   FALSE if there were any errors, TRUE if there weren't any.
 */
function _cdn_admin_validate_synced_files_db($synced_files_db, $name) {
  // Validate the file name.
  if (strpos($synced_files_db, CDN_DAEMON_SYNCED_FILES_DB) === FALSE) {
    form_set_error($name, t('The synced files database should have the file name %name.', array('%name' => CDN_DAEMON_SYNCED_FILES_DB)));
    return FALSE;
  }

  // Validate the entered synced files database.
  if (!file_exists($synced_files_db)) {
    form_set_error($name, t('The synced files database does not exist.'));
    return FALSE;
  }
  else {
    if (!@fopen($synced_files_db, 'r')) {
      form_set_error($name, t('The synced files database could not be opened for reading.'));
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * Helper function to check if the requirements of the CDN integration module
 * have been met. If any requirement errors exist, they are aggregated into
 * a single error message and are subsequently displayed.
 *
 * @return
 *   The number of requirement errors.
 */
function _cdn_admin_check_requirements() {
  // Check run-time requirements of the CDN integration module.
  include_once './includes/install.inc';
  $requirements = cdn_requirements('runtime');

  // Filter out the requirement errors and display these, with links back to
  // the admin/reports/status page.
  $errors = array();
  foreach ($requirements as $requirement => $details) {
    if ($details['severity'] == REQUIREMENT_ERROR) {
      $errors[] = l($details['title'], 'admin/reports/status') . '<br />' . $details['description'];
    }
  }
  if (!empty($errors)) {
    $args = array(
      '!error-list' => theme('item_list', $errors),
    );
    drupal_set_message(t('Please fix the following problems with the installation of the CDN integration module:<br />!error-list', $args), 'error');
  }

  return count($errors);
}
