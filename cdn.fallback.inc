<?php
// $Id$

/**
 * @file
 * Fallback when hook_file_url_alter() is not available (i.e. when the core
 * patch is not installed or when not using Pressflow): use the Parallel
 * module's logic (with some adaptations to be able to use the CDN module's
 * logic (in particular: cdn_file_url_alter()) to alter file URLs via the
 * theme layer.
 */


/**
 * Implementation of hook_theme_registry_alter().
 *
 * Make CDN's page preprocess function run *after* everything else's, so that
 * a theme can't call drupal_get_js() and mess everything up.
 *
 * @param &$theme_registry
 *   The entire cache of theme registry information, post-processing.
 */
function cdn_theme_registry_alter(&$theme_registry) {
  if (isset($theme_registry['page'])) {
    // If CDN's preprocess function is there already, remove it.
    if ($key = array_search('cdn_preprocess_page', $theme_registry['page']['preprocess functions'])) {
      unset($theme_registry['page']['preprocess functions'][$key]);
    }
    // Now tack it on at the end so it runs after everything else.
    $theme_registry['page']['preprocess functions'][] = 'cdn_preprocess_page';
  }
}

/**
 * Implementation of template_preprocess_page().
 *
 * @param &$variables
 *   Array containing various page elements.
 */
function cdn_preprocess_page(&$variables) {
  global $base_url;
  $root = preg_quote(base_path()) . '|' . preg_quote($base_url);

  // CSS
  $pattern = "#href=\"(($root)(.*?\.css)(\?.*)?)\"#";
  _cdn_preprocess_page_helper($variables['styles'], $pattern, 0, 3, 4, 'href="', '"');

  // JS
  $pattern = "#src=\"(($root)(.*?\.js)(\?.*)?)\"#";
  _cdn_preprocess_page_helper($variables['scripts'], $pattern, 0, 3, 4, 'src="', '"');
  _cdn_preprocess_page_helper($variables['closure'], $pattern, 0, 3, 4, 'src="', '"');

  // Images
  $skip_keys = array('styles', 'scripts', 'zebra', 'id', 'directory', 'layout', 'head_title', 'base_path', 'front_page', 'head', 'body_clases');
  foreach ($variables as $key => $value) {
    if (!in_array($key, $skip_keys) && is_string($value) && !empty($value)) {
      $matches = array();
      $pattern = "#(<img\s+|<img\s+[^>]*\s+)src\s*=\s*\"(($root)([^\"]*)(\?.*)?)\"#i";
      _cdn_preprocess_page_helper($variables[$key], $pattern, 0, 4, 5, '<img src="', '"');
    }
  }
}

/**
 * Helper function for cdn_preprocess_page().
 *
 * @param &$html
 *   The HTML in which file URLs will be altered.
 * @param $pattern
 *   A regular expression pattern to apply to the subject.
 * @param $search_index
 *   The index of the search string in the array of regexp matches.
 * @param $path_index
 *   The index of the file path in the array of regexp matches.
 * @param $querystring_index
 *   The index of (an optional) query string in the array of regexp matches.
 * @param $prefix
 *   $search_index will be replaced by $prefix, plus the altered file URL,
 *   plus the @suffix.
 * @param $suffix
 *   See $prefix.
 */
function _cdn_preprocess_page_helper(&$html, $pattern, $search_index, $path_index, $querystring_index, $prefix, $suffix) {
  // Find a match against the given pattern.
  preg_match_all($pattern, $html, $matches);

  // Generate replacements to alter file URLs.
  $searches = array();
  $replacements = array();
  for ($i = 0; $i < count($matches[0]); $i++) {
    $search = $matches[$search_index][$i];
    $path = $matches[$path_index][$i];
    cdn_file_url_alter($path);
    $searches[]     = $search;
    $replacements[] = $prefix . $path . $matches[$querystring_index][$i] . $suffix;
  }

  // Apply the generated replacements ton the subject.
  $html = str_replace($searches, $replacements, $html);
}
