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
  cdn_log(count($files) ." files will be synchronized.");

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

        // Fail immediately, otherwise we'd start uploading files into the
        // root directory!
        return $stats;
      }
    }

    // Sync deletion.
    $stats['deletes'] = _ftp_cdn_cron_sync_deletion($fs, $files_unique);

    // Sync files. Validate only by checking existence and filesize.
    $files_on_server = _ftp_cdn_get_filelist($fs, '.');
    $num = 0;
    $total = count($files_unique);
    foreach ($files as $file => $file_size) {
      $num++;
      $file_size_formatted = number_format($file_size / 1024, 1);

      // If we had to alter the original file, make sure we use that file as
      // the source file. We also have to update the filesize then.
      $source_file = $files_updated[$file];
      if ($file != $source_file) {
        $file_size = filesize($source_file);
      }

      $target_file = $files_unique[$file];

      if ((isset($files_on_server[$target_file]) && $files_on_server[$target_file] == $file_size)) {
        $stats['exists']++;
        $stats['exists_bytes'] += $file_size;
        cdn_log("[$num/$total] Already exists: $target_file ($file_size_formatted KB).");
      }
      else {
        if (_ftp_cdn_cron_create_path($fs, $target_file) && ftp_put($fs, "./$target_file", "./$source_file", FTP_BINARY)) {
          $stats['uploads']++;
          $stats['uploaded_bytes'] += $file_size;
          cdn_log("[$num/$total] Uploaded $target_file ($file_size_formatted KB).");
        }
        else {
          $stats['uploads_failed']++;
          cdn_log("[$num/$total] Failed to upload $target_file.");
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
  $files_on_server = _ftp_cdn_get_filelist($fs, '.');

  if (!empty($files_on_server)) {
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
  }
  return $deletes;
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

function _ftp_cdn_get_filelist($fs, $path) {
  static $filelist = array();

  if (!isset($filelist[$path])) {
    $filelist[$path] = _ftp_cdn_parse_filesizes_recursive_rawlist(ftp_rawlist($fs, $path, TRUE));
    cdn_log('Retrieved full list of files and file sizes for path "'. $path .'".');
  }
  return $filelist[$path];
}

function _ftp_cdn_parse_filesizes_recursive_rawlist($raw_list) {
  $list = array();

  foreach ($raw_list as $file) {
    if ($file == '') {
      continue;
    }
    elseif (substr($file, strlen($file) - 1, strlen($file) ) == ':') {
      $path = substr($file, 0, strlen($file) - 1);
      
      // Remove leading "./".
      if (strstr($path, './') !== FALSE ) {
        $path = substr($path, 2);
      }
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
