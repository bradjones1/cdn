<?php
// $Id$

/**
 * @file
 * The ftp module allows you to
 */


define('DEBUG', TRUE);
define('DEBUG_OUTPUT', 'html');


/**
 * Implementation of pseudo-hook hook_cdn_cron_perform_sync().
 */
function ftp_cdn_cron_perform_sync($filters, $settings) {
  extract($filters);
  extract($settings);

  $stats = cdn_cron_init_stats();

  $files = cdn_cron_get_files_to_sync($pattern, $ignored_dirs, $dirs_all_files, $exclude_pattern);
  cdn_log("List of files (". count($files). ") that should be synced has been created.");

  // Connect.
  $fs = ftp_connect($host, $port);

  // Log on.
  if ($fs && ftp_login($fs, $user, $pass)) {
    cdn_log("Logged on to {$host}:{$port}.");

    // Change directory if necessary.
    if (isset($remote_path) && !empty($remote_path)) {
      if (ftp_chdir($fs, $remote_path)) {
        cdn_log("Changed directory to remote path ($remote_path).");
      }
      else {
        cdn_log("Failed to changed directory to remote path ($remote_path).");
      }
    }

    // Sync deletion.
    _ftp_cdn_cron_sync_deletion($fs, $files);

    // Sync files. Validate only by checking existence and filesize.
    foreach ($files as $file) {
      $file_size = filesize($file);
      $file_size_formatted = _cdn_filesize($file_size);

      if (_ftp_cdn_cron_file_exists($fs, $file, $file_size)) {
        $stats['exists']++;
        $stats['exists_bytes'] += $file_size;
        cdn_log("Already exists: $file ($file_size_formatted).");
      }
      else {
        if (_ftp_cdn_cron_create_path($fs, $file) && ftp_put($fs, "./$file", "./$file", FTP_BINARY)) {
          $stats['uploads']++;
          $stats['uploaded_bytes'] += $file_size;
          cdn_log("Uploaded $file ($file_size_formatted).");
        }
        else {
          $stats['uploads_failed']++;
          cdn_log("Failed to upload $file.");
        }
      }
    }

    // Close the FTP stream.
    ftp_close($fs);
    cdn_log("Logged out.");
  }
  else {
    cdn_log("Failed to log on to $host:$port.");
  }

  return $stats;
}


//----------------------------------------------------------------------------
// Private functions.

function _ftp_cdn_cron_sync_deletion($fs, $files, $path = '') {
  static $files_string;
  
  if (!isset($files_string)) {
    $files_string = implode('|', $files);
  }

  $list = ftp_nlist($fs, '.');
  foreach ($list as $file) {
    if (strpos($files_string, $file) === FALSE) {
      $deleted = FALSE;

      if (ftp_size($fs, $file) == -1) {
        ftp_chdir($fs, $file);
        _ftp_cdn_cron_sync_deletion($fs, $files, "$path/$file");
        ftp_cdup($fs);
        $deleted = ftp_rmdir($fs, $file);
      }
      else {
        $deleted = ftp_delete($fs, $file);
      }

      if ($deleted) {
        cdn_log("Deleted $path/$file.");
      }
      else {
        cdn_log("Failed to deleted $path/$file.");
      }
    }
  }
}

function _ftp_cdn_cron_file_exists($fs, $file_path, $file_size) {
  static $file_sizes;
  
  if (!isset($file_sizes)) {
    cdn_log('Retrieved file sizes of all files beneath the remote path. This happens only once.');
    $file_sizes = _cdn_parse_filesizes_recursive_rawlist(ftp_rawlist($fs, '.', TRUE));
  }

  return (isset($file_sizes[$file_path]) && $file_sizes[$file_path] == $file_size);
}

function _ftp_cdn_cron_create_path($fs, $file) {
  $success = TRUE;

  $dirs = explode('/', $file);
  unset($dirs[count($dirs) - 1]); // Remove the file from the dirs array.

  // Create the path.
  for ($i = 0; $success && $i < count($dirs); $i++) {
    if (!in_array($dirs[$i], ftp_nlist($fs, '.'))) {
      $success = ftp_mkdir($fs, $dirs[$i]);
    }
    $success = ftp_chdir($fs, $dirs[$i]);
  }

  // Go back to the original working directory.
  $chdir = '';
  for ($i = 0; $success && $i < count($dirs); $i++) {
    if ($chdir) {
      $chdir .= '/';
    }
    $chdir .= '..';
  }
  $success = ftp_chdir($fs, $chdir);

  return $success;
}

function _cdn_parse_filesizes_recursive_rawlist($raw_list) {
  $list = array();

  foreach ($raw_list as $file) {
    if ($file == '') {
      continue;
    }
    elseif (substr($file, strlen($file) - 1, strlen($file) ) == ':') {
      $path = substr($file, 0, strlen($file) - 1);
    }
    else {
      $parts = preg_split("/[\s]+/", $file, 9);
      $is_file = $parts[0][0] != 'd';
      $file_name = $parts[8];
      $file_size = $parts[4];
      
      if ($is_file) {
        $list[$path .'/'. $file_name] = $file_size;
      }
    }
  }
  return $list;
}

function _cdn_filesize($size) {
  return round($size / 1024, 1) . ' KB';
}
