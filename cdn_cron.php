<?php
// $Id$

/**
 * @file
 * Handles incoming requests to fire off CDN synchronization cron jobs.
 */

include_once './includes/bootstrap.inc';
drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
$cdn_dir = drupal_get_path('module', 'cdn');
require_once "$cdn_dir/cdn.inc";
require_once "$cdn_dir/cdn_cron.inc";
cdn_cron_run();
