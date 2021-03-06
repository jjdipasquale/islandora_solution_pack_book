<?php

function book_add_pages(&$form_state, $pid) {
  $path = drupal_get_path('module', 'islandora_book');
  drupal_add_js($path . '/js/book.js');
  drupal_add_css($path . '/css/islandora_book.css');
  $list = array(
    l(t("Append or Insert Pages"), "islandora/book/upload/$pid"),
   // l(t("Create Digital Page"), "islandora/book/borndigital/$pid")
  );
  $form['link'] = array(
    '#type' => 'item',
    '#value' => l(t('Return to book view'), "fedora/repository/$pid"),
  );
  $form['page_add'] = array(
    '#attributes' => array('class' => 'page_fieldset'),
    '#type' => 'fieldset',
    '#collapsed' => FALSE,
    '#collapsible' => true,
  );
  $form['page_add']['links'] = array(
    '#value' => theme_item_list($list),
  );


  return $form;
}

/**
 * book add pages form
 * @param array $form_state
 * @param string $pid
 * @param string $page_cmodel
 * @param string $relation
 * @return array
 */
function book_add_pages_form(&$form_state, $pid, $page_cmodel = 'islandora:pageCModel', $relation = 'isMemberOf') {
  $file = (isset($form_state['values']['ingest-file-location']) ? $form_state['values']['ingest-file-location'] : '');
  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  module_load_include('inc', 'islandora_book', 'management/IslandoraFileReorderTable');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  $path = drupal_get_path('module', 'islandora_book');
  drupal_add_js($path . '/js/book.js');

  $table_data = islandora_file_reorder_table($pid);
  $table = $table_data['table'];
  $filecount = $table_data['filecount'];
  $languages = islandora_book_get_available_tesseract_languages();

  $item = new Fedora_Item($pid);
  $book_title = $item->objectProfile->objLabel;
  $toggle_link = l(t("Reorder Files"), '#');
  $prompt = t("Add page(s) to @title", array('@title' => $book_title));
  $submit_prompt = t('Ingest Pages');
  if ($page_cmodel == 'islandora:bd_pageCModel') {
    $prompt = t("Create digital page for @title", array('@title' => $book_title));
    $submit_prompt = t("Create page");
  }
  $pages = TRUE;
  $current_page_count = count(get_page_pids($pid));
  if ($current_page_count == 0) {
    $pages = FALSE;
  }
  $message = t("This sequence currently has @count pages. Additional pages will be appended at the end of the sequence by default. !break", array("@count" => $current_page_count, '!break' => '<br />'));
  $message .= t("Choose a number lower than @count to insert page(s) at a specific location in the sequence.", array("@count" => $current_page_count, '!break' => '<br />'));
  if ($filecount > 0) {
    $form['intro'] = array(
      '#type' => 'item',
      '#title' => '<span id ="file_label">' . $toggle_link . "</span",
      '#description' => t("Adjust ingest order of uploaded files")
    );

    $form['table'] = array(
      '#prefix' => '<div id ="file_view">',
      'table' => $table,
      '#suffix' => '</div>',
    );

    $form['alpha_sort'] = array(
      '#type' => 'checkbox',
      '#title' => t('Sort by input file names?'),
      '#description' => t("If checked files will be sorted alphanumerically to determine page order.")
    );
  }

  $form['page_add'] = array(
    '#type' => 'item',
    '#title' => $prompt,
  );

  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );
  $form['book_title'] = array(
    '#type' => 'hidden',
    '#value' => $book_title,
  );
  $form['page_cmodel'] = array(
    '#type' => 'hidden',
    '#value' => $page_cmodel,
  );

  $form['page_count'] = array(
    '#type' => 'hidden',
    '#value' => $current_page_count,
  );

  $form['relation'] = array(
    '#type' => 'hidden',
    '#value' => $relation,
  );
  if ($pages) {
    $form['insertion_point'] = array(
      '#type' => 'textfield',
      '#title' => t('Last sequence number'),
      '#default_value' => $current_page_count,
      '#description' => $message,
      '#size' => 5,
    );
  }
  if ($page_cmodel == 'islandora:pageCModel') {

    $form['preprocess'] = array(
      '#title' => t('Preprocessing for typescripts?'),
      '#description' => t('Will add additional processing for typewritten text.  This script grayscales and sharpens an ingested .tiff before performing OCR processes. <br />
     This has been shown to improve OCR results for some images. Use with caution. Process can be lengthly and is resource intensive. <br />
    If your image is already gray, this script will not run'),
      '#type' => 'checkbox',
    );


    $form['language'] = array(
      '#title' => t('Language'),
      '#type' => 'select',
      '#options' => $languages,
      '#selected' => 'eng',
    );
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#weight' => 10,
    '#value' => $submit_prompt,
  );
  return $form;
}

/**
 * book add pages form validate
 * @param array $form
 * @param array $form_state
 */
function book_add_pages_form_validate($form, &$form_state) {
  $_SESSION['manage_chosen'] = TRUE;
  if (isset($form_state['values']['insertion_point'])) {
    $insertion_point = $form_state['values']['insertion_point'] + 0;
    $count = $form_state['values']['page_count'] + 0;
    if (!is_int($insertion_point) || $insertion_point > $count || $insertion_point < 0) {
      form_set_error('insertion_point', "Number must be a positive integer less than or equal to $count");
      return;
    }
  }

  unset($_SESSION['manage_chosen']);
}

/**
 * book add pages form submit
 * @param array $form
 * @param array $form_state
 */
function book_add_pages_form_submit($form, &$form_state) {

  global $base_path;

  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $pageCModel = $form_state['values']['page_cmodel'];
  $pid = $form_state['values']['pid'];
  $preprocess = $form_state['values']['preprocess'];
  $start_page = (isset($form_state['values']['insertion_point'])) ? $form_state['values']['insertion_point'] + 1 : 1;
  $dirName = str_replace(':', '_', $pid);
  $tmpDir = file_directory_path() . '/' . $dirName . '/';
  $relationship = $form_state['values']['relation'];
  $language = (isset($form_state['values']['language'])) ? $form_state['values']['language'] : 'eng';

  if ($pageCModel == 'islandora:pageCModel') {
    $file_list = array();
    foreach ($form_state['values']['table']['rows'] as $row) {
      $pos = (strlen($row['pos']) > 0) ? $row['pos'] : 0;
      $pos = intval($pos);
      $file_list[$pos] = $row['file'];
    }
    if ($form_state['values']['alpha_sort']) {
      sort($file_list);
    }
    else {
      ksort($file_list);
    }

    ibook_create_page_objects($pid, $file_list, $form_state['values']['page_cmodel'], $relationship, $tmpDir, $start_page, $language, $preprocess);
  }
  if ($pageCModel == 'islandora:bd_pageCModel') {
    renumber_pages($start_page, $pid, 1);
    $path = drupal_get_path('module', 'islandora_book') . '/images/';


    $book_title = $form_state['values']['book_title'];
    $page_title = "$book_title - Page " . sprintf("%03d", $start_page);
    $parts = explode(':', $pid);
    $nextPid = FEDORA_ITEM::get_next_PID_in_namespace($parts[0]);
    $page_item = Fedora_Item::ingest_new_item($nextPid, 'A', t('Digital Page'));
    $page_item->add_relationship('hasModel', 'islandora:bd_pageCModel', FEDORA_MODEL_URI);
    $page_item->add_relationship('isSequenceNumber', $start_page, 'http://islandora.ca/ontology/relsext#', 3);
    $page_item->add_relationship('isPageNumber', $start_page, 'http://islandora.ca/ontology/relsext#', 3);
    $page_item->add_relationship('isSection', '1', 'http://islandora.ca/ontology/relsext#', 3);
    $page_item->add_relationship('isPageOf', $pid, 'http://islandora.ca/ontology/relsext#');
    $page_item->add_relationship($relationship, $pid);
    $page_item->add_datastream_from_file($path . 'Borndigital_tn.png', 'TN', 'Thumbnail', 'image/png', 'M');
    $page_item->add_datastream_from_file($path . 'Borndigital_Fullsize.jpg', 'JPEG', 'JPEG Image', 'image/jpg', 'M');
    $page_item->add_datastream_from_file($path . 'Borndigital.jp2', 'JP2', "Compressed JP2", 'image/jp2', 'M');
    $page_item->add_datastream_from_file($path . 'Borndigital_Fullsize.tiff', 'TIFF', 'Archival Image', 'image/jpg', 'M');
    $page_item->add_datastream_from_string('Born digital - not yet edited', 'OCR', 'OCR', 'text/plain', 'M');
    drupal_goto("fedora/repository/$pid");
  }
}

/**
 * Create a page object for each element in $image_list as a child object of $pid
 * @param string $pid
 * @param array $image_list
 * @param string $page_cmodel
 * @param string $relation
 * @param string $tmpDir
 * @return boolean
 */
function ibook_create_page_objects($pid, $image_list = array(), $page_cmodel = 'islandora:pageCModel', $relation = 'isMemberOfCollection', $tmpDir, $start_page, $language = 'eng', $preprocess) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/ObjectHelper');
  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  $book = new Fedora_Item($pid);
  if (!$book->exists()) {
    return FALSE;
  }
  $dirName = str_replace(':', '_', $pid);
  $page_pids = array();
  $image_count = count($image_list);
  $batch = array(
    'title' => "Creating $image_count page objects.",
    'operations' => array(),
    'file' => drupal_get_path('module', 'islandora_book') . '/management/add_pages.inc',
    'init_message' => t('Preparing data...'),
    'progress_message' => t('@current of @total tasks completed.'),
  );
  $page_num = $start_page;
  $book_title = $book->objectProfile->objLabel;
  $batch['operations'][] = array('wrap_renumber_pages', array($start_page, $pid, $image_count));
  foreach ($image_list as $image_path) {
    $batch['operations'][] = array('ibook_add_single_page_object', array($pid, $image_path, $page_num, "$book_title - Page " . sprintf("%03d", $page_num), $page_cmodel, $relation, $language, $preprocess));
    $page_num++;
  }
  $batch['operations'][] = array('deleteDirectory', array($tmpDir));
  if (variable_get('islandora_create_book_pdf', FALSE)) {
    $batch['operations'][] = array('makePDF', array($pid, $image_list, $tmpDir));
  }
  $batch['operations'][] = array('deleteDirectory', array($tmpDir));
  $collection = get_collection_from_pid($pid);
  batch_set($batch);
  batch_process("fedora/repository/$pid");
  $objectHelper = new ObjectHelper;
  $dc_xml = $objectHelper->getStream($pid, 'DC');
  if (!preg_match('/<dc:type>ingested<\/dc:type>/', $dc_xml)) {
    $xml = new DOMDocument();
    $xml->loadXML($dc_xml);
    $root = $xml->documentElement;
    $typeNode = $xml->createElement('dc:type', 'ingested');
    $typeNode = $xml->createElementNS("http://purl.org/dc/elements/1.1/", 'dc:type', 'ingested');
    $root->appendChild($typeNode);
    $book->modify_datastream_by_value($xml->saveXML(), 'DC', 'Dublin core', 'text/xml', TRUE, "Added Type of Ingested", FALSE);
  }
  $collection = get_collection_from_pid($pid);
  drupal_goto('fedora/repository/' . $collection[0]);
}

/**
 * ibook add single page object
 * @param string $book_pid
 * @param string $page_file
 * @param int $page_num
 * @param string $page_title
 * @param string $page_cmodel
 * @param string $relation
 */
function ibook_add_single_page_object($book_pid, $page_file, $page_num = 1, $page_title = NULL, $page_cmodel = 'islandora:pageCModel', $relation = 'isMemberOf', $language, $preprocess) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $parts = explode(':', $book_pid);
  $base_file_name = pathinfo($page_file, PATHINFO_BASENAME);
  $ext = pathinfo($page_file, PATHINFO_EXTENSION);
  $base_file_name = str_replace(".$ext", "", $base_file_name);
  $nextPid = FEDORA_ITEM::get_next_PID_in_namespace($parts[0]);
  $page_item = Fedora_Item::ingest_new_item($nextPid, 'A', $base_file_name);
  $page_item->add_relationship('hasModel', $page_cmodel, FEDORA_MODEL_URI);
  $page_item->add_relationship('isSequenceNumber', $page_num, 'http://islandora.ca/ontology/relsext#', 3);
  $page_item->add_relationship('isPageNumber', $page_num, 'http://islandora.ca/ontology/relsext#', 3);
  $page_item->add_relationship('isPageOf', $book_pid, 'http://islandora.ca/ontology/relsext#');
  $page_item->add_relationship('isSection', '1', 'http://islandora.ca/ontology/relsext#', 3);
  $page_item->add_relationship('isMemberOf', $book_pid, RELS_EXT_URI);

  //add TECHMD stream if enabled

  if (variable_get('enable_fits', FALSE) == TRUE) {
    exec(variable_get('fits_path', '/usr/local/bin/fits.sh') . ' -i ' . escapeshellarg($page_file) . '', $TECHMD);
    $TECHMD = implode($TECHMD);
    $xml = simplexml_load_string($TECHMD);
    $xml->registerXPathNamespace('fits', 'http://hul.harvard.edu/ois/xml/ns/fits/fits_output');
    $image_height = $xml->xpath('//fits:imageHeight');
    $image_height = (string) $image_height[0];
    $image_width = $xml->xpath('//fits:imageWidth');
    $image_width = (string) $image_width[0];
    $page_item->add_datastream_from_string($TECHMD, 'TECHMD', 'TECHMD', 'text/xml', 'M');
    $page_item->add_dsid_relationship('JPEG', 'islandora-book:height', $image_height, 'info:islandora/islandora-system:def/pageinfo#', 3);
    $page_item->add_dsid_relationship('JPEG', 'islandora-book:width', $image_width, 'info:islandora/islandora-system:def/pageinfo#', 3);
  }
  // add original image
  if (strtolower($ext) == 'tif') {
    $ext = 'TIFF';
  }
  module_load_include('inc', 'fedora_repository', 'MimeClass');
  $mime_helper = new MimeClass();
  $mimetype = $mime_helper->get_mimetype($page_file);

  $page_item->add_datastream_from_file($page_file, strtoupper($ext), 'Archival IMAGE', $mimetype, 'M');
  $page_file = preg_replace('/\/\//', '/', $page_file);


  //add compresed jp2 for viewer
  if (variable_get('islandora_book_create_images', TRUE)) {
    if (strtolower($ext) == 'tif' || strtolower($ext) == 'tiff' || strtolower($ext) == 'jpg' || strtolower($ext) == 'jpeg') {
      exec('kdu_compress -i "' . $page_file . '" -o "' . $page_file . '.jp2' . '" -rate 0.5 Clayers=1 Clevels=7 "Cprecincts={256,256},{256,256},{256,256},{128,128},{128,128},{64,64},{64,64},{32,32},{16,16}" "Corder=RPCL" "ORGgen_plt=yes" "ORGtparts=R" "Cblk={32,32}" Cuse_sop=yes', $output, $returnValue);
      if ($returnValue == '0') {
        $attempts = 0;
        $success = NULL;
        while ($attempts < 10 && !isset($success)) {
          $success = $page_item->add_datastream_from_file("$page_file.jp2", 'JP2', "Compressed JP2", 'image/jp2', 'M');
          if (!isset($success)) {
            time_nanosleep(0, 500000000);
          }
          $attempts++;
        }
      }
      unlink("$page_file.jp2");
    }
    //add thumbnail image
    $cmd = "convert {$page_file}[0] -colorspace RGB -thumbnail 120x $page_file-tn.jpg";
    exec($cmd, $output, $returnValue);
    if ($returnValue == '0') {
      if ($page_num == 1) {
        $book_item = new Fedora_Item($book_pid);
        $DSIDS = $book_item->get_datastreams_list_as_array();
        if (array_key_exists("TN", $DSIDS)) {
          $book_item->purge_datastream('TN');
        }
        $book_item->add_datastream_from_file("$page_file-tn.jpg", 'TN', 'Thumbnail', 'image/jpeg', 'M');
      }
      $page_item->add_datastream_from_file("$page_file-tn.jpg", 'TN', 'Thumbnail', 'image/jpeg', 'M');
      unlink("$page_file-tn.jpg");
    }
    //add medium jpeg
    exec("convert -resize 800 \"{$page_file}\" \"$page_file-med.jpg\"", $output, $returnValue);
    if ($returnValue == '0') {
      $page_item->add_datastream_from_file("$page_file-med.jpg", 'JPEG', "JPEG image", 'image/jpeg', 'M');
      unlink("$page_file-med.jpg");
    }
  }
  //perform OCR using Tesseract, add text datastream

  if (variable_get('islandora_book_do_ocr', TRUE)) {
    if ($preprocess) {
      $command = "gimp -d -i -b '(munge-typeset-ocr $page_file $page_file.png 5.0 10.0 0.0 200.0 5.0)' -b '(gimp-quit 0)'";
      exec($command, $output, $returnVal);
      if ($returnVal == 0) {
        $page_file = "$page_file.png";
      }
    }
    $executable = variable_get('islandora_ocr_path', '/usr/bin/tesseract');
    $returnValue = "";
    $output = "";
    $command = "$executable $page_file $page_file -l $language";
    exec("$executable $page_file $page_file -l $language", $output, $returnValue);
    if ($returnValue == '0') {
      $textfile = "$page_file.txt";
      $page_item->add_datastream_from_file("$page_file.txt", 'OCR', "Scanned Text", 'text/plain', 'M');
      unlink($textfile);
    }
    else {
      $page_item->add_datastream_from_string('No OCR available', 'OCR', 'Scanned Text', 'text/plain', 'M');
    }
    islandora_book_viewer_create_hocr($executable, $page_file, $language, &$page_item);
  }
  unlink($page_file);
}

/**
 * creates hocr from a supplied image, also lowercase the text so it can
 * be used for highlighting search results.
 * @param string $executable
 * @param string $page_file
 * @param string $lang
 * @return boolean
 */
function islandora_book_viewer_create_hocr($executable, $page_file, $lang, &$object) {
  exec("$executable $page_file $page_file -l $lang hocr", $output, $returnValue);
  $datastreams = $object->get_datastreams_list_as_array();
  if ($returnValue == '0') {
    if (!is_file("$page_file.html")) {
      unlink("$page_file.txt");
      return FALSE;
    }
    $html_string = file_get_contents("$page_file.html");
    if (empty($html_string)) {
      return FALSE;
    }
    $object->add_datastream_from_file("$page_file.html", 'RAW_HOCR', "RAW_HOCR", 'text/xml', 'M');
    $proc = new XsltProcessor();
    //convert the hocr from html 4.01 transitional to xhtml and lower case the text
    $doc_type = '<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">';
    $xml_string = str_replace($doc_type, '<?xml version="1.0" encoding="UTF-8"?>', $html_string);
    $input = new DomDocument();
    $input->loadXML($xml_string);
    $path = drupal_get_path('module', 'islandora_book');
    $xsl = new DomDocument();
    $test = $xsl->load($path . '/xsl/hocrToLowerhocr.xsl');
    if (!$test) {//can't run transform so leaving
      return FALSE;
    }
    $xsl = $proc->importStylesheet($xsl);
    $newdom = $proc->transformToDoc($input);
    $content = $newdom->saveXML();
    file_put_contents("$page_file.xml", $content);
    if (array_key_exists('ENCODED_OCR', $datastreams)) {
      $object->purge_datastream('ENCODED_OCR');
    }
    $object->add_datastream_from_file("$page_file.xml", 'ENCODED_OCR', "ENCODED_OCR", 'text/xml', 'M');
    $datastreams = $object->get_datastreams_list_as_array();
    if (array_key_exists('ENCODED_OCR', $datastreams)) {
      $returnValue = TRUE;
    }
    file_delete("$page_file.xml");
    return $returnValue;
  }
  return NULL;
}

/**
 * delete directory
 * @param string $dir
 * @return boolean
 */
function deleteDirectory($dir) {
  if (!file_exists($dir))
    return TRUE;
  if (!is_dir($dir))
    return unlink($dir);
  foreach (scandir($dir) as $item) {
    if ($item == '.' || $item == '..')
      continue;
    if (!deleteDirectory($dir . DIRECTORY_SEPARATOR . $item))
      return FALSE;
  }
  return rmdir($dir);
}

/**
 * make PDF
 * @param string $book_pid
 * @param array $file_list
 * @param string $tmpDir
 */
function makePDF($book_pid, $file_list, $tmpDir) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $book_item = new Fedora_Item($book_pid);
  $outputFile = "${tmpDir}book.pdf";
  $tiffs = array();
  $cmdString = 'convert -compress JPEG -density 150';
  foreach ($file_list as $tiff) {
    $cmdString .= " $tiff";
  }
  $cmdString .= " $outputFile";
  exec($cmdString, $output, $returnValue);
  $datastreams = $book_item->get_datastreams_list_as_array();
  if ($returnValue == '0') {
    if (array_key_exists('PDF', $datastreams)) {
      $book_item->purge_datastream('PDF');
    }
    $book_item->add_datastream_from_file($outputFile, 'PDF', "PDF of Book", 'application/pdf', 'M');
  }
}

//deprecated - to be deleted on approval
function page_delete(&$form_state, $pid) {
  module_load_include('inc', 'islandora_book', 'management/IslandoraBookPageTable');
  $table = islandora_book_page_table($pid);

  if (user_access('delete pages')) {
    $form['pid'] = array(
      '#type' => 'hidden',
      '#value' => $pid,
    );
    $form['delete_page'] = array(
      '#title' => "Permanently Delete Pages from this Book",
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );

    $form['delete_page']['table'] = array(
      'table' => $table,
    );


    $form['delete_page']['confirm']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Delete Selected Pages'),
      '#id' => 'delete_pages',
    );
  }
  return $form;
}

//deprecated - to be deleted on approval
function page_delete_submit($form, &$form_state) {

  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  $book_pid = $form_state['values']['pid'];
  $page_pids = @array_filter($form_state['values']['table']['selections']);
  $count = count($page_pids);
  $batch = array(
    'title' => t("Deleting @page_count pages", array('@page_count' => $count)),
    'operations' => array(),
    'file' => drupal_get_path('module', 'islandora_book') . '/management/add_pages.inc',
    'init_message' => t('Preparing data...'),
    'progress_message' => t('@current of @total tasks completed.'),
  );

  foreach ($page_pids as $page_pid) {
    $batch['operations'][] = array('islandora_book_delete_single_page', array($page_pid));
  }
  $batch['operations'][] = array('wrap_post_deletion_renumber_pages', array($book_pid));
  batch_set($batch);
  batch_process("fedora/repository/$book_pid");
}

function islandora_book_delete_single_page($page_pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  global $user;
  $item = new Fedora_Item($page_pid);
  $item->purge($pid . ' deleted by user identified as ' . $user->name);
}

/**
 * Wrapper to call function from utilities page
 * Batch will not call functions from multiple files
 * @param string $book_pid
 */
function wrap_post_deletion_renumber_pages($book_pid) {
  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  post_deletion_renumber_pages($book_pid);
}

/**
 * Wrapper to call function from utilities page
 * Batch will not call functions from multiple files
 * @param string $book_pid
 */
function wrap_renumber_pages($start_page, $pid, $image_count) {
  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  renumber_pages($start_page, $pid, $image_count);
}

function islandora_book_get_dimensions($pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $item = new fedora_item($pid);
  $TECHMD = $item->get_datastream_dissemination('TECHMD');
  $xml = simplexml_load_string($TECHMD);
  $xml->registerXPathNamespace('fits', 'http://hul.harvard.edu/ois/xml/ns/fits/fits_output');
  $image_height = $xml->xpath('//fits:imageHeight');
  $image_height = (string) $image_height[0];
  $image_width = $xml->xpath('//fits:imageWidth');
  $image_width = (string) $image_width[0];
}
