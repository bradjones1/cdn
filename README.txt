// $Id$

Applying the Drupal core patch
------------------------------

You *must* apply this patch!

First, change the directory to the Drupal root directory.

You can apply the included Drupal core patch like this:
  patch -p0 < drupal_core_cdn_integration.patch

To undo the patch:
  patch -p0 -R < drupal_core_cdn_integration.patch


Applying the theme patch
------------------------

You *must* apply this patch to *every* theme that's being used on your website!

Repeat this process for every theme: first, change the directory to the
directory of the theme. Applying the patch is identical to the example above,
only with a different filename.


Setting up CDN sync filters
---------------------------
- paths: This is an array of paths (each path being relative to the Drupal
         root directory) on which this filter should be applied.
- pattern: A regular expression that will be used to filter the files in each
           directory. Like the $mask parameter in file_scan_directory().
- ignored_dirs: An array of directories that should be ignored in each
                directory. Like the $nomask parameter in file_scan_directory().
- unique: Determines how the uniqueness will be applied. You can set it to
          'filename', which will alter the filename, or 'common parent
          directory', which will alter the path of the file. The latter is
          strongly recommended for themes, since it will not break URLs in
          CSS files.
- unique_method: The method that should be used to generate unique filenames.
                 Currently supported: 'mtime' (the file's mtime property), 
                 'md5' (md5 hash of the file) or 'md5 of mtimes' (md5 hash of
                 the concatenated mtimes of a set of files). This last option
                 is only available if you have set the unique property to
                 'common parent directory'.


Configuring the $conf array in settings.php
-------------------------------------------

This is my configuration:

$conf = array(
  'cdn_url' => 'http://wimleers.cachefly.com/wimleers.com',
  'cdn_sync_filters' => array(
    0 => array(
      'paths' => array('misc', 'profiles', 'modules', 'sites/all/modules', 'sites/default/modules'),
      'pattern' => '.*\.(js|css|gif|png|jpg|jpeg|svg|swf)$',
      'ignored_dirs' => array('.', '..', 'CVS', '.svn'),
      'unique' => 'filename',
      'unique_method' => 'mtime',
    ),
    1 => array(
      'paths' => array('sites/wimleers.com/files'),
      'pattern' => '.*',
      'ignored_dirs' => array('.', '..', 'CVS', '.svn'),
      'unique' => 'filename',
      'unique_method' => 'mtime',
    ),
    2 => array(
      'paths' => array('sites/default/themes/garland-customized'),
      'pattern' => '.*\.(js|css|gif|png|jpg|jpeg)$',
      'ignored_dirs' => array('.', '..', 'CVS', '.svn'),
      'unique' => 'common parent directory',
      'unique_method' => 'md5 of mtimes',
    ),
  ),
  'cdn_sync_method' => 'ftp',
  'cdn_sync_method_settings' => array(
    'host' => 'ftp.cachefly.com',
    'remote_path' => 'wimleers.com',
    'port' => 21,
    'user' => 'user',
    'pass' => 'pass',
  ),
);
