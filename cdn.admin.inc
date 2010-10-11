<?php
// $Id$

/**
 * @file
 * Settings administration UI.
 */


//----------------------------------------------------------------------------
// Form API callbacks.

/**
 * Form definition; general settings.
 */
function cdn_admin_general_settings_form(&$form_state) {
  ctools_include('dependent');
  require_once drupal_get_path('module', 'cdn') . '/cdn.requirements.inc';

  // Prevent requirement errors from showing up twice.
  if (empty($form_state['post'])) {
    _cdn_admin_check_requirements();
  }

  $form['#submit'] = array('cdn_admin_settings_submit_show_cache_warning');


  // Check which CDN integration mechanism is being used.
  $integration_method = _cdn_requirements_get_integration_mechanism();
  switch ($integration_method) {
    case 'fallback':
      $desc = t('No core patch applied — falling back to theme layer to alter
                 file URLs.<br />
                 <u>Note:</u> this mechanism is unable to alter all file URLs.
                 <strong>If you want complete CDN coverage, you should either
                 apply the included Drupal core patch or switch to
                 <a href="http://pressflow.org/">Pressflow</a>.</strong>');
      break;
    case 'pressflow':
      $desc = t("This is a Pressflow site — no core patch necessary.");
      break;
    case 'core patch':
      $desc = t("Core patch applied!");
      break;
  }
  $form['integration_method'] = array(
    '#type' => 'item',
    '#title' => t('Integration method'),
    '#value' => $desc,
  );

  $form[CDN_STATUS_VARIABLE] = array(
    '#type' => 'radios',
    '#title' => t('Status'),
    '#description' => t(
      "If you don't want to use the CDN to serve files to your visitors yet,
      but you do want to see if it's working well for your site, then enable
      testing mode.<br />Users will only get the files from the CDN if they
      have the %cdn-testing-mode-permission permission.",
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
 * Form definition; details.
 */
function cdn_admin_details_form(&$form_state) {
  ctools_include('dependent');

  // Prevent requirement errors from showing up twice.
  if (empty($form_state['post'])) {
    _cdn_admin_check_requirements();
  }

  $form['#submit'] = array('cdn_admin_settings_submit_show_cache_warning');

  $form[CDN_MODE_VARIABLE] = array(
    '#type' => 'radios',
    '#title' => t('Mode'),
    '#description' => t(
      "It is essential that you understand the <a href=\"!cdn-article-url\">key
      properties of a CDN</a>, most importantly the differences between an
      <em>Origin Pull CDN</em> and a <em>Push CDN</em>. If you understand that,
      continue to make a choice between the two supported modes:
      !mode-explanation-list", array(
        '!cdn-article-url' => 'http://wimleers.com/article/key-properties-of-a-cdn',
        '!mode-explanation-list' => theme('item_list', array(
          t("<strong>Origin Pull mode</strong> replaces your domain name with
            the Origin Pull CDN's domain name in the file URL."),
          t("<strong>File Conveyor mode</strong> uses the <a href=\"http://fileconveyor.org\">File Conveyor</a>
            daemon to synchronize files and can be used with both Origin Pull
            and Push CDNs. If you're using an Origin Pull CDN and want to
            optimize files before they're synced to the CDN, it is also
            recommended to use this mode.")
        )),
      )
    ),
    '#required' => TRUE,
    '#options'  => array(
      CDN_MODE_BASIC    => t('Origin Pull'),
      CDN_MODE_ADVANCED => t('File Conveyor'),
    ),
    '#default_value' => variable_get(CDN_MODE_VARIABLE, CDN_MODE_BASIC),
  );


  $form['settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Mode-specific settings'),
    '#description' => t('Settings specific to the mode you have currently selected.'),
  );


  //
  // Basic mode settings.
  //

  $format_variables = array(
    '@format-url'        => t('<URL>'),
    '@format-extensions' => t('<extensions>'),
  );
  $format_explanation_list_items = array(
    t("<strong>@format-url</strong> is the CDN URL that should be used. E.g.:
      '<code>http://cdn-a.com</code>'.", $format_variables),
    t("<strong>@format-extensions</strong> is an <strong>optional</strong>
      setting to limit which files should be served from a CDN. E.g.:
      '<code>.css .jpg .jpeg .png</code>'.", $format_variables),
  );
  $example_list_items = array(
    t('This would serve <em>all</em> files from a single CDN:
      <pre><code>http://cdn-a.com</code></pre>'),
    t("This would serve CSS and images from CDN A, .zip files from CDN B and
       everything else from CDN C:
       <pre><code>http://cdn-a.com|.css .jpg .jpeg .png\nhttp://cdn-b.com|.zip\nhttp://cdn-c.com</code></pre>"),
  );
  $form['settings'][CDN_BASIC_MAPPING_VARIABLE] = array(
    '#type'          => 'textarea',
    '#title'         => t('CDN mapping'),
    '#description'   => t("Define which files are mapped to which CDN.<br />
                           <br />
                           Enter one mapping per line, in the format <strong>@format-url[|@format-extensions]</strong>:<br />
                           !format-explanation-list
                           <br />
                           Sample mappings:
                           !example-list", $format_variables + array(
                             '!format-explanation-list' => theme('item_list', $format_explanation_list_items),
                             '!example-list' => theme('item_list', $example_list_items),
                          )),
    '#size'          => 35,
    '#default_value' => variable_get(CDN_BASIC_MAPPING_VARIABLE, ''),
    '#process'       => array('ctools_dependent_process'),
    '#dependency'    => array('radio:' . CDN_MODE_VARIABLE => array(CDN_MODE_BASIC)),
  );

  //
  // Advanced mode settings.
  //

  $form['settings'][CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE] = array(
    '#type'          => 'textfield',
    '#title'         => t('Synced files database'),
    '#description'   => t('Enter the full path to File Conveyor\'s synced files database file.'),
    '#size'          => 60,
    '#default_value' => variable_get(CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE, ''),
    '#process'       => array('ctools_dependent_process'),
    '#dependency'    => array('radio:' . CDN_MODE_VARIABLE => array(CDN_MODE_ADVANCED)),
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

  if (variable_get(CDN_MODE_VARIABLE, CDN_MODE_BASIC) == CDN_MODE_ADVANCED) {
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
  }

  $form['https'] = array(
    '#type' => 'fieldset',
    '#title' => t('HTTPS'),
    '#description' => t('When a page is being served via HTTPS and the CDN
                         does not support HTTPS, then the file will not be
                         served from the CDN, because it would make the visit
                         insecure.<br/>
                         When your CDN <em>does</em> support HTTPS, enable the
                         corresponding setting below, and your CDN will be
                         used on HTTPS pages.'),
  );
  $form['https'][CDN_HTTPS_SUPPORT_VARIABLE] = array(
    '#type'          => 'checkbox',
    '#title'         => t('CDN supports HTTPS support'),
    '#default_value' => variable_get(CDN_HTTPS_SUPPORT_VARIABLE, FALSE),
    '#process'       => array('ctools_dependent_process'),
  );

  $path_explanation = t(
    "Enter one file pattern per line. The '*' character is a wildcard.<br />
    Example file patterns are <code>*.js</code> for all JavaScript files and
    <code>mytheme/*.css</code> for all CSS files in the <code>mytheme</code>
    directory."
  );

  $form['exceptions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Exceptions'),
    '#description' => t('Which files should be served from a CDN is not as
                         simple as it seems: there are bound to be exceptions.
                         You can easily define those exceptions here, either
                         by file URL or for logged in users.'),
  );

  $form['exceptions']['path'] = array(
    '#type' => 'fieldset',
    '#title' => t('File URL'),
    '#description' => t("Files that are marked to not be served from the CDN
    because of a match in the blacklist, can be overridden to be served from
    the CDN after all, if they have a match in the whitelist.<br />
    <br />
    <strong>All JavaScript files are excluded by default. This is necessary
    to ensure a painless out-of-the-box experience.</strong> For maximum
    performance, you should only exclude problematic JavaScript files.
    <br />
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
    requests.</strong>"),
    '#collapsible' => TRUE,
  );

  $form['exceptions']['path'][CDN_EXCEPTION_PATH_BLACKLIST_VARIABLE] = array(
    '#type' => 'textarea',
    '#title' => t('Blacklist'),
    '#default_value' => variable_get(CDN_EXCEPTION_PATH_BLACKLIST_VARIABLE, CDN_EXCEPTION_PATH_BLACKLIST_DEFAULT),
    '#description' => $path_explanation,
  );
  $form['exceptions']['path'][CDN_EXCEPTION_PATH_WHITELIST_VARIABLE] = array(
    '#type' => 'textarea',
    '#title' => t('Whitelist'),
    '#default_value' => variable_get(CDN_EXCEPTION_PATH_WHITELIST_VARIABLE, CDN_EXCEPTION_PATH_WHITELIST_DEFAULT),
    '#description' => $path_explanation,
  );

  $form['exceptions']['auth_users'] = array(
    '#type' => 'fieldset',
    '#title' => t('Authenticated users'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );

  $form['exceptions']['auth_users'][CDN_EXCEPTION_AUTH_USERS_BLACKLIST_VARIABLE] = array(
    '#type'          => 'textarea',
    '#title'         => t('Blacklist'),
    '#default_value' => variable_get(CDN_EXCEPTION_AUTH_USERS_BLACKLIST_VARIABLE, CDN_EXCEPTION_AUTH_USERS_BLACKLIST_DEFAULT),
    '#description'   => $path_explanation,
  );

  return system_settings_form($form);
}

/**
 * Default validate callback for the details form.
 */
function cdn_admin_details_form_validate($form, &$form_state) {
  // If ctools_dependent supported required dependent form items, this ugly
  // piece of code would not be necessary.
  if ($form_state['values'][CDN_MODE_VARIABLE] == CDN_MODE_BASIC) {
    if (empty($form_state['values'][CDN_BASIC_MAPPING_VARIABLE])) {
      form_error($form['settings'][CDN_BASIC_MAPPING_VARIABLE], t('!name field is required.', array('!name' => $form['settings'][CDN_BASIC_MAPPING_VARIABLE]['#title'])));
    }
  }
  else {
    if (empty($form_state['values'][CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE])) {
      form_error($form['settings'][CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE], t('!name field is required.', array('!name' => $form['settings'][CDN_ADVANCED_SYNCED_FILES_DB_VARIABLE]['#title'])));
    }
  }

  // When in advanced mode, validate the synced files DB.
  if ($form_state['values'][CDN_MODE_VARIABLE] == CDN_MODE_ADVANCED) {
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
 * Helper function to check if the requirements of the CDN module have been
 * met. If any requirement errors exist, they are aggregated into a single
 * error message and are subsequently displayed.
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
