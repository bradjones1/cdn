<?php
// $Id$

/**
 * @file
 * FTP synchronization plugin for the CDN integration module.
 */


define('DEBUG', FALSE);
define('DEBUG_OUTPUT', 'html');


/**
 * Implementation of pseudo-hook hook_cdn_cron_perform_sync().
 */
function ftp_cdn_cron_perform_sync($files, $files_unique, $files_updated, $stats, $settings) {
  extract($settings);
  cdn_log(count($files). " files will be synchronized.");

  $fs = ftp_connect($host, $port);
  if ($fs && ftp_login($fs, $user, $pass)) {
    cdn_log("Logged on to {$host}:{$port}.");

    // Change directory if necessary.
    if (isset($remote_path) && !empty($remote_path)) {
      if (ftp_chdir($fs, $remote_path)) {
        cdn_log("Changed directory to remote path ($remote_path).");
      }
      else {
        cdn_log("Failed to change directory to remote path ($remote_path).");
      }
    }

    // Sync deletion.
    $stats['deletes'] = _ftp_cdn_cron_sync_deletion($fs, $files_unique);

    // Sync files. Validate only by checking existence and filesize.
    foreach ($files as $file => $file_size) {
      $file_size_formatted = number_format($file_size / 1024, 1);

      if (_ftp_cdn_cron_file_exists($fs, $files_unique[$file], $file_size)) {
        $stats['exists']++;
        $stats['exists_bytes'] += $file_size;
        cdn_log("Already exists: $files_unique[$file] ($file_size_formatted KB).");
      }
      else {
        if (_ftp_cdn_cron_create_path($fs, $files_updated[$file]) && ftp_put($fs, "./$files_updated[$file]", "./$file", FTP_BINARY)) {
          $stats['uploads']++;
          $stats['uploaded_bytes'] += $file_size;
          cdn_log("Uploaded $files_unique[$file] ($file_size_formatted KB).");
        }
        else {
          $stats['uploads_failed']++;
          cdn_log("Failed to upload $files_unique[$file].");
        }
      }
    }

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


function _ftp_cdn_cron_sync_deletion($fs, $files) {
  $deletes = 0;
  $files_on_server = _cdn_parse_filesizes_recursive_rawlist(ftp_rawlist($fs, '.', TRUE));
  $files_to_be_deleted = array_diff(array_keys($files_on_server), array_values($files));

  foreach ($files_to_be_deleted as $filepath) {
    $dirs = explode('/', $filepath);
    $file = end($dirs);
    unset($dirs[count($dirs) - 1]);
    $path = implode('/', $dirs);

    ftp_chdir($fs, $path);
    if (ftp_delete($fs, $file)) {
      cdn_log("Deleted $path/$file.");
      $deletes++;
    }
    else {
      cdn_log("Failed to deleted $path/$file.");
    }

    // Go back to the original working directory, deleting empty directories
    // on the go.
    $success = TRUE;
    for ($i = count($dirs) - 1; $i >= 0; $i--) {
      ftp_chdir($fs, '..');
      $list = ftp_nlist($fs, $dirs[$i]);
      if ($success && empty($list)) {
        if ($success = ftp_rmdir($fs, $dirs[$i])) {
          cdn_log("Deleted empty directory ". implode('/', $dirs) .".");
          $deletes++;
        }
        else {
          cdn_log("Failed to deleted empty directory ". implode('/', $dirs) .".");
        }
        unset($dirs[$i]);
      }
    }
  }

  return $deletes;
}

function _ftp_cdn_cron_file_exists($fs, $file_path, $file_size) {
  $file_sizes = _cdn_parse_filesizes_recursive_rawlist(ftp_rawlist($fs, '.', TRUE));
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
    ftp_chdir($fs, $dirs[$i]);
  }

  // Go back to the original working directory.
  $chdir = '';
  for ($i = 0; $i < count($dirs); $i++) {
    if ($chdir) {
      $chdir .= '/';
    }
    $chdir .= '..';
  }
  ftp_chdir($fs, $chdir);

  return $success;
}

function _cdn_parse_filesizes_recursive_rawlist($raw_list) {
  static $list;
  
  if (!isset($list)) {
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
    cdn_log('Retrieved full list of files  and file sizes.');
  }

  return $list;
}
