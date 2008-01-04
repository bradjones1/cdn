<?php
// $Id$

/**
 * @file
 * Handles incoming requests to fire off CDN synchronization cron jobs.
 */

include_once './includes/bootstrap.inc';
drupal_bootstrap(DRUPAL_BOOTSTRAP_CONFIGURATION);
drupal_bootstrap(DRUPAL_BOOTSTRAP_DATABASE);
drupal_bootstrap(DRUPAL_BOOTSTRAP_ACCESS);
require_once dirname(drupal_get_filename('module', 'cdn')) .'/cdn_cron.inc';
cdn_cron_run();
