<?php
// $Id$

/**
 * @file
 * Basic functions for CDN synchronization cron.
 */


/**
 * Executes a CDN synchronization cron run when called
 *
 * Graciously stolen from Drupal's includes/common.inc.
 */
function cdn_cron_run() {
  // If not in 'safe mode', increase the maximum execution time:
  if (!ini_get('safe_mode')) {
    set_time_limit(240);
  }
  
  // TRICKY: Create a faux user object to satisfy watchdog queries.
  global $user;
  $user = new stdClass();
  $user->uid = 0;

  // Fetch the CDN synchronization cron semaphore
  $semaphore = variable_get('cdn_cron_semaphore', FALSE);

  if ($semaphore) {
    if (time() - $semaphore > 3600) {
      // Either cron has been running for more than an hour or the semaphore
      // was not reset due to a database error.
      watchdog('cdn_cron', 'CDN synchronization cron has been running for more than an hour and is most likely stuck.', WATCHDOG_ERROR);

      // Release CDN synchronization cron semaphore
      variable_del('cdn_cron_semaphore');
    }
    else {
      // CDN synchronization cron is still running normally.
      watchdog('cdn_cron', 'Attempting to re-run CDN synchronization cron while it is already running.', WATCHDOG_WARNING);
    }
  }
  else {
    // Register shutdown callback
    register_shutdown_function('cdn_cron_cleanup');

    // Lock CDN synchronization cron semaphore
    variable_set('cdn_cron_semaphore', time());

    // Get variables.
    $sync_method = variable_get('cdn_sync_method', 'ftp');
    $sync_filters = variable_get('cdn_sync_filters', FALSE);
    $sync_method_settings = variable_get('cdn_sync_method_settings', FALSE);

    // Get parameters for perform_sync hook.
    $files = cdn_cron_get_files_to_sync($sync_filters['pattern'], $sync_filters['ignored_dirs'], $sync_filters['dirs_all_files'], $sync_filters['exclude_pattern']);
    $files_unique = cdn_cron_get_unique_filenames($files);
    $stats = cdn_cron_init_stats();

    // Perform the CDN synchronization using the configured method. Default to
    // the FTP  method. 
    require_once dirname(drupal_get_filename('module', 'cdn')) ."/sync_plugins/$sync_method.inc";
    $sync_hook = $sync_method .'_cdn_cron_perform_sync';
    timer_start('cdn_sync');
    $stats = $sync_hook($files, $files_unique, $stats, $sync_method_settings);
    $timer = timer_stop('cdn_sync');

    // Build message.
    $duration = round($timer['time'] / 1000, 1);
    extract($stats);
    $exists_kbytes = number_format($exists_bytes / 1024, 1);
    $uploaded_kbytes = number_format($uploaded_bytes / 1024, 1);
    $message = "<ul>";
    $message = "<li>Method: <em>$sync_method</em></li>";
    $message .= "<li>Duration: <em>$duration</em> seconds</li>";
    $message .= "<li>Deleted: <em>$deletes</em> files</li>";
    if ($exists_bytes) {
      $message .= "<li>No action: <em>$exists</em> files ($exists_kbytes KB)</li>";
    }
    else {
      $message .= "<li>No action: <em>$exists</em> files</li>";
    }
    if ($uploaded_bytes) {
      $message .= "<li>Uploaded: <em>$uploads</em> files ($uploaded_kbytes KB)</li>";
    }
    else {
      $message .= "<li>Uploaded: <em>$uploads</em> files</li>";
    }
    $message .= '</ul>';

    // Log to watchdog.
    watchdog('cdn_cron', "CDN synchronization cron run completed.<br />$message", WATCHDOG_NOTICE);
    if ($uploads_failed) {
      watchdog('cdn_cron', "$uploads_failed file uploads have failed.", WATCHDOG_ERROR);
    }

    // Record CDN synchronization cron time and statistics message for usage
    // in the status report at admin/logs/status.
    variable_set('cdn_cron_last', time());
    variable_set('cdn_cron_last_stats', $message);
    
    // Record which files have been synchronized. We have to know this to be
    // able to generate CDN URLs only for files that have been synchronized.
    variable_set('cdn_files_synced', $files_unique);

    // Release CDN synchronization cron semaphore
    variable_del('cdn_cron_semaphore');

    // Return TRUE so other functions can check if it did run successfully
    return TRUE;
  }
}

/**
 * Shutdown function for CDN cron cleanup.
 *
 * Also graciously stolen from Drupal's includes/common.inc.
 */
function cdn_cron_cleanup() {
  // See if the semaphore is still locked.
  if (variable_get('cdn_cron_semaphore', FALSE)) {
    watchdog('cron', 'CDN synchronization cron run exceeded the time limit and was aborted.', WATCHDOG_WARNING);

    // Release CDN synchronization cron semaphore
    variable_del('cdn_cron_semaphore');
  }
}


//----------------------------------------------------------------------------
// Functions used in the synchronization process.

/**
 * Generates the list of files that has to be sync, based on a file name
 * pattern, a list of ignored directories, a list of directories of which any
 * file will be included, and an exclude pattern.
 *
 * @return
 *   An array of which the keys are filepaths and the values are filesizes.
 */
function cdn_cron_get_files_to_sync($pattern, $ignored_dirs, $dirs_all_files, $exclude_pattern) {
  // Get all files that match the pattern and are not within one of the
  // ignored directories.
  $files = _cdn_cron_scan_directory('.', $pattern, $ignored_dirs);

  // Add all files that are in any of the directories of which all files must
  // be added.
  foreach ($dirs_all_files as $dir) {
    $files += _cdn_cron_scan_directory($dir, '.*', $ignored_dirs);
  }

  // Get relative filepaths.
  $files = array_flip(array_map('cdn_clean_filepath', array_flip($files)));

  // Remove files if the directory they were in is one of 
  if (strlen($exclude_pattern)) {
    foreach ($files as $key => $file) {
      if (preg_match($exclude_pattern, $file)) {
        unset($files[$key]);
      }
    }
  }

  return $files;
}

function cdn_cron_get_unique_filenames($files) {
  return array_map('cdn_unique_filename', array_combine(array_keys($files), array_keys($files)));
}

/**
 * Initializes the stats array.
 *
 * @return
 *   The stats array.
 */
function cdn_cron_init_stats() {
  $stats = array(
    'deletes' => 0,
    'exists' => 0,
    'exists_bytes' => 0,
    'uploads' => 0,
    'uploads_failed' => 0,
    'uploaded_bytes' => 0,
  );

  return $stats;
}

/**
 * Simple logging function for sync plugin debugging purposes only.
 */
function cdn_log($message) {
  $output = 'console';

  if (DEBUG_OUTPUT !== FALSE) {
    $output = DEBUG_OUTPUT;
  }

  if (DEBUG) {
    $time = '['. date('H:i:s') .'] ';

    switch ($output) {
      case 'html':
        print '<pre>'. $time . $message ."</pre>\n";
        break;

      case 'console':
      default:
        print $time . $message ."\n";
        break;
    }
  }
}


//----------------------------------------------------------------------------
// Private functions.

/**
 * Stolen from Drupal core. Simplified for CDN cron usage. Parameter order is
 * unchanged, so you can look at the official file_scan_directory()
 * documentation but think away the last parameters.
 *
 * @see file_scan_directory
 */
function _cdn_cron_scan_directory($dir, $pattern, $ignored_dirs = array('.', '..', 'CVS', '.svn')) {
  $files = array();

  if (is_dir($dir) && $handle = opendir($dir)) {
    while ($file = readdir($handle)) {
      if (!in_array($file, $ignored_dirs) && $file[0] != '.') {
        if (is_dir("$dir/$file")) {
          $files += _cdn_cron_scan_directory("$dir/$file", $pattern, $ignored_dirs);
        }
        elseif (ereg($pattern, $file)) {
          $files["$dir/$file"] = filesize("$dir/$file");
        }
      }
    }

    closedir($handle);
  }

  return $files;
}
