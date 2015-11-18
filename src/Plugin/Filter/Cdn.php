<?php

/**
 * @file
 * Contains \Drupal\cdn\Plugin\Filter\Cdn.
 */

namespace Drupal\cdn\Plugin\Filter;

use Drupal\Core\Plugin\ContainerFactoryPluginInterface;
use Drupal\filter\FilterProcessResult;
use Drupal\filter\Plugin\FilterBase;
use Symfony\Component\DependencyInjection\ContainerInterface;
use Symfony\Component\HttpFoundation\RequestStack;

/**
 * Provides a filter to rewrite file URLs to point to the CDN.
 *
 * @Filter(
 *   id = "cdn",
 *   title = @Translation("Serve assets from a CDN"),
 *   type = Drupal\filter\Plugin\FilterInterface::TYPE_TRANSFORM_IRREVERSIBLE
 * )
 */
class Cdn extends FilterBase implements ContainerFactoryPluginInterface {

  /**
   * The request stack.
   *
   * @var \Drupal\Core\Entity\EntityManagerInterface
   */
  protected $requestStack;

  /**
   * Constructs a \Drupal\cdn\Plugin\Filter\Cdn object.
   *
   * @param array $configuration
   *   A configuration array containing information about the plugin instance.
   * @param string $plugin_id
   *   The plugin_id for the plugin instance.
   * @param mixed $plugin_definition
   *   The plugin implementation definition.
   * @param \Symfony\Component\HttpFoundation\RequestStack $request_stack
   *   The request stack.
   */
  public function __construct(array $configuration, $plugin_id, $plugin_definition, RequestStack $request_stack) {
    $this->requestStack = $request_stack;
    parent::__construct($configuration, $plugin_id, $plugin_definition);
  }

  /**
   * {@inheritdoc}
   */
  static public function create(ContainerInterface $container, array $configuration, $plugin_id, $plugin_definition) {
    return new static(
      $configuration,
      $plugin_id,
      $plugin_definition,
      $container->get('request_stack')
    );
  }

  /**
   * {@inheritdoc}
   */
  public function process($text, $langcode) {
    return (new FilterProcessResult($text))
      ->setProcessedText($this->alterImageFileUrls($text))
      // See ::generateUrlPrefixRegEx().
      ->addCacheContexts(['url.site']);
      //->addCacheableDependency($cdn_settings);
  }

  /**
   * Alters image file URLs in a piece of HTML.
   *
   * @param &$html
   *   The HTML in which image file URLs will be altered.
   *
   * @return string
   *   The updated HTML.
   */
  protected function alterImageFileUrls($html) {
    $url_prefix_regex = $this->generateUrlPrefixRegEx();

    // TRICKY: order matters!

    // Image file URLs of <a> tags that wrap <img> tags.
    $pattern = "#((<a\s+|<a\s+[^>]*\s+)href\s*=\s*[\"|'])($url_prefix_regex)([^\"|^'|^\?]*)()(\?[^\"|^']*)?";
    $pattern .= "("; // Capture everything after the path.
    $pattern .= "([\"|'][^>]*)>"; // End of opening <a> tag.
    $pattern .= "((<img\s+|<img\s+[^>]*\s+)src\s*=\s*[\"|'])([^\"|^'|^\?]*)()(\?[^\"|^']*)?([\"|'])"; // Wrapped <img> tag.
    $pattern .= ")#i";
    $result = $this->alterFileUrl($html, $pattern, 0, 4, 6, 1, 7, 11);

    // Image file URLs in <img> tags.
    $pattern = "#((<img\s+|<img\s+[^>]*\s+)src\s*=\s*[\"|'])($url_prefix_regex)([^\"|^'|^\?]*)()(\?[^\"|^']*)?([\"|'])#i";
    $result = static::alterFileUrl($result , $pattern, 0, 4, 6, 1, 7);

    return $result;
  }

  /**
   * Generates the URL prefix regular expression, that supports all possible
   * types of file URLs: root-relative, protocol-relative and absolute URLs.
   *
   * This depends on the base URL, and hence varies by the 'url.site' cache
   * context.
   */
  protected function generateUrlPrefixRegEx() {
    static $url_prefix_regex;

    $request = $this->requestStack->getCurrentRequest();

    // It's okay to statically cache this across all requests, because all
    // requests must have the same scheme, HTTP host and base URL anyway.
    if (!isset($url_prefix_regex)) {
      $url_prefixes = array(
        preg_quote($request->getBaseUrl() . '/') . '(?!/)', // Root-relative URL.
        // Note:  root-relative URL that isn't a protocol-relative URL. The
        // negative lookahead for a trailing slash is relaly only necessary
        // when this site is installed in the document root (i.e. when the base
        // path equals "/"), because this would otherwise match *all* protocol-
        // relative URLs, also those that already point to the CDN.
        preg_quote('//' . $request->getHttpHost() . $request->getBaseUrl() . '/'), // Protocol-relative URL.
        preg_quote($request->getScheme() . '://' . $request->getHttpHost() . $request->getBaseUrl() . '/'), // Absolute URL.
      );

      $regexes = array();
      $farfuture = preg_quote('cdn/farfuture/');
      foreach ($url_prefixes as $url_prefix) {
        $regexes[] = $url_prefix . '(?!' . $farfuture . ')';
      }

      // The URL prefix regex that will match all URLs to the current site,
      // except for those that already point to Far Future expiration URLs.
      $url_prefix_regex = implode('|', $regexes);
    }

    return $url_prefix_regex;
  }

  /**
   * Alter the file URLs in a piece of HTML given a regexp pattern and some
   * additional parameters.
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
   *   plus the @suffix. If numeric, then it is assumed to be the index of the
   *   prefix in the array of regexp matches.
   * @param $suffix
   *   See $prefix.
   * @param $comparison_index
   *   The index of a comparison path whose file extension should match the file
   *   extension of the path located at $path_index.
   *
   * @return string
   *   The updated HTML.
   */
  protected function alterFileUrl($html, $pattern, $search_index, $path_index, $querystring_index, $prefix, $suffix, $comparison_index = FALSE) {
    // Find a match against the given pattern.
    preg_match_all($pattern, $html, $matches);

    // Generate replacements to alter file URLs.
    $searches = array();
    $replacements = array();
    for ($i = 0; $i < count($matches[0]); $i++) {
      $search = $matches[$search_index][$i];
      $path = $matches[$path_index][$i];

      $prefix_string = (is_numeric($prefix)) ? $matches[$prefix][$i] : $prefix;
      $suffix_string = (is_numeric($suffix)) ? $matches[$suffix][$i] : $suffix;

      // Compare the filename in the path with that of another index. The file
      // URL only is rewritten if it matches.
      if ($comparison_index) {
        $comparison = $matches[$comparison_index][$i];

        // Calculate length of extension.
        $path_ext_pos = strrpos($path, '.');
        $ext_length = -1 * (strlen($path) - $path_ext_pos);

        if (substr($path, $ext_length) !== substr($comparison, $ext_length)) {
          continue;
        }
      }

      // Store the current path as the old path, then let cdn_file_url_alter()
      // do its magic by invoking all file_url_alter hooks. When the path hasn't
      // changed and is not already root-relative or protocol-relative, then
      // generate a file URL as Drupal core would: prepend the base path.
      $old_path = $path;
      drupal_alter('file_url', $path);
      if ($path == $old_path && drupal_substr($path, 0, 1) != '/' && drupal_substr($path, 0, 2) != '//') {
        $path = base_path() . $path;
      }

      $searches[]     = $search;
      $replacements[] = $prefix_string . $path . $matches[$querystring_index][$i] . $suffix_string;
    }

    // Apply the generated replacements ton the subject.
    return str_replace($searches, $replacements, $html);
  }

}
