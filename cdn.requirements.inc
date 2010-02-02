<?php

/**
 * Generates a requirement for the given patch.
 */
function _cdn_requirements_generate_requirement_for_patch(&$requirements, $patch_name, $title) {
  $t = get_t();

  $patterns_function = '_cdn_requirements_' . $patch_name . '_patch_patterns';
  $patterns = $patterns_function();
  $unpatched_files = _cdn_requirements_check_patch_applied($patterns);

  $key = 'cdn_' . $patch_name . '_patch';

  $requirements[$key]['title'] = $title;
  if (count($unpatched_files) == 0) {
    $requirements[$key] += array(
      'severity'    => REQUIREMENT_OK,
      'value'       => $t('Applied'),
    );
  }
  else {
    $requirements[$key] += array(
      'description' => $t('This patch has not been applied to the following files:') . '<br />' . theme('item_list', $unpatched_files) . '<br />' . t('Please consult the installation instructions in the included README.txt.'),
      'severity'    => REQUIREMENT_ERROR,
      'value'       => $t('Not or incompletely applied.'),
    );
  }
}

/**
 * Check if a patch has been applied, given a set of patterns.
 */
function _cdn_requirements_check_patch_applied($patterns) {
  $drupal_root = realpath('.');
  $patched = TRUE;
  $unpatched_files = array();
  foreach ($patterns as $kind => $details) {
    foreach ($details as $file_info => $patterns) {
      foreach ($patterns as $pattern) {
        if ($kind == 'core') {
          $filename = $file_info;
          $full_path = $drupal_root . '/' . $filename;
        }
        else {
          list($name, $filename) = explode('|', $file_info);
          $full_path = $drupal_root . '/' . drupal_get_path($kind, $name) . '/' . $filename;
        }

        $match = preg_match('|' . preg_quote($pattern) . '|m', file_get_contents($full_path));
        $patched = $patched && $match;

        // Remember unpatched files.
        if (!$match && !in_array($full_path, $unpatched_files)) {
          $unpatched_files[] = $full_path;
        }
      }
    }
  }
  return $unpatched_files;
}

/**
 * Generate patterns for a patch, given the full path to a patch. This
 * effectively parses the patch and stores it in a meaningful structure.
 */
function _cdn_requirements_generate_patterns_for_patch($full_path) {
  $file_kinds = array(
    '/cvs/drupal/drupal/modules'                => 'module',
    '/cvs/drupal/drupal/themes'                 => 'theme',
    '/cvs/drupal/drupal'                        => 'core',
    '/cvs/drupal-contrib/contributions/modules' => 'module',
    '/cvs/drupal-contrib/contributions/themes'  => 'theme',
  );


  $fp = fopen($full_path, 'r');
  $patch_block = '';
  while (!feof($fp) ) {
    $line = fgets($fp);

    // Check if the current line indicates the next file.
    if (substr($line, 0, 7) == 'Index: ') {
      $file_to_patch = substr($line, 7, strlen($line) - 7 - 1);
    }

    // Find out which kind of file this is: a core file, a module file or a
    // theme file.
    if (substr($line, 0, 10) == 'RCS file: ') {
      $rcs_file = substr($line, 10);
      foreach ($file_kinds as $patch_rcs_file => $patch_type) {
        if (substr($rcs_file, 0, strlen($patch_rcs_file)) == $patch_rcs_file) {
          $kind = $patch_type;
          // For files of the module or theme kind, also store the exact name.
          if ($kind == 'module' || $theme == 'theme') {
            $start = strlen($patch_rcs_file) + 1;
            $module_or_theme = substr($rcs_file, $start, strpos($rcs_file, '/', $start) - $start);
          }
          break;
        }
      }
    }

    // Finally, store the lines that have been added (+): concatenate them in
    // a "patch block".
    if ($line[0] == '+' && ($line[1] == ' ' | $line[1] == "\n")) {
      $patch_block .= substr($line, 1);
    }
    // When we encounter a line that has not been added (+), the current patch
    // block has ended and we should store it as a pattern.
    else {
      if ($patch_block != '') {
        if ($kind == 'core') {
          $patterns[$patch_type][$file_to_patch][] = $patch_block;
        }
        else {
          $patterns[$patch_type][$module_or_theme . '|' . $file_to_patch][] = $patch_block;
        }
      }
      $patch_block = '';
    }
  }
  fclose($fp);

  return $patterns;
}

/**
 * Generates the patterns for the core patch.
 */
function _cdn_requirements_core_patch_patterns() {
  return _cdn_requirements_generate_patterns_for_patch(drupal_get_path('module', 'cdn') . '/patches/drupal6.patch');
}

/**
 * Generates the patterns for the ImageCache patch.
 */
function _cdn_requirements_imagecache_patch_patterns() {
  return _cdn_requirements_generate_patterns_for_patch(drupal_get_path('module', 'cdn') . '/patches/imagecache.patch');
}
