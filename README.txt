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


Configuring the $conf array in settings.php
-------------------------------------------

Make sure you have the following values in your $conf array:

$conf = array(
  ...
  'cdn_url' => 'http://user.cachefly.com/some/subdir',
  'cdn_sync_filters' => array(
    'pattern' => '.*\.(js|css)$',
    'exclude_pattern' => '/^sites\/default/',
    'ignored_dirs' => array('.', '..', 'CVS', '.svn'),
    'dirs_all_files' => array('sites/yoursite.com/files'),
  ),
  'cdn_unique_filenames_method' => 'mtime',
  'cdn_sync_method' => 'ftp',
);


If you have set the "cdn_sync_method" setting to "ftp", then also :

$conf = array(
  ...
  'cdn_sync_method_settings' => array(
    'host' => 'ftp.cachefly.com',
    'remote_path' => 'some/subdir',
    'port' => 21,
    'user' => 'user',
    'pass' => 'pass',
  ),
);
