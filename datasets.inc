<?php

function get_dataset_form() {
	$form['name'] = array(
		'#type' => 'textfield',
		'#title' => t('Dataset Name'),
		'#size' => 60,
		'#required' => TRUE,
	);
	$form['path'] = array(
		'#type' => 'textfield',
		'#title' => t('HDFS Folder Name'),
		'#size' => 60,
	);
	$form['compilerpath'] = array(
		'#type' => 'textfield',
		'#title' => t('Compiler Folder Name'),
		'#size' => 60,
	);
	$form['opts'] = array(
		'#type' => 'textfield',
		'#title' => t('Runtime Arguments'),
		'#size' => 60,
	);
	$form['enabled'] = array(
		'#type' => 'checkbox',
		'#title' => t('Release dataset to users'),
		'#default_value' => NULL,
	);
	$form['submit'] = array(
		'#type' => 'submit',
	);

	return $form;
}

function boa_dataset_add_form($form, &$form_state) {
	$f = get_dataset_form();
	$f['path']['#required'] = TRUE;
	$f['compilerpath']['#required'] = TRUE;
	$f['submit']['#value'] = t('Add Dataset');
	return $f;
}

function boa_dataset_add_form_validate($form, &$form_state) {
	$path = $form_state['values']['path'];
	$out = system(hadoop_exec_path() . ' fs -ls /repcache/' . escapeshellcmd($path) . '/projects.seq > /dev/null', $retval);
	if ($retval != 0)
		form_set_error('path', t('HDFS Folder Name ') . "'$path'" . t(' does not exist or is not a valid input dataset.'));

	$compiler_base = boa_variable_get('compiler_path') . '/';
	$compiler_path = $form_state['values']['compilerpath'];
	if (!is_dir($compiler_base . $compiler_path))
		form_set_error('compilerpath', t('Compiler Folder Name ') . "'$compiler_path'" . t(' does not exist.'));
	else if (!is_file($compiler_base . $compiler_path . '/dist/boa-runtime.jar'))
		form_set_error('compilerpath', t('Compiler Folder Name ') . "'$compiler_path'" . t(' does not contain a runtime JAR.'));
	else if (!is_file($compiler_base . $compiler_path . '/dist/boa-compiler.jar'))
		form_set_error('compilerpath', t('Compiler Folder Name ') . "'$compiler_path'" . t(' does not contain a compiler JAR.'));
}

function boa_dataset_add_form_submit($form, &$form_state) {
	$query = db_insert('boa_input');
	$query->fields(array(
		'name' => $form_state['values']['name'],
		'path' => $form_state['values']['path'],
		'run_opts' => isset($form_state['values']['opts']) ? $form_state['values']['opts'] : NULL,
		'compiler_path' => $form_state['values']['compilerpath'],
		'enabled' => (int)$form_state['values']['enabled'],
	));
	$query->execute();

	drupal_set_message(t('Dataset was added'), 'status');
	drupal_goto('boa/datasets/add');
	drupal_exit();
}

function boa_dataset_edit_form($form, &$form_state, $input_id) {
	$f = get_dataset_form();
	$f['submit']['#value'] = t('Update Dataset');
	$f['id'] = array(
		'#type' => 'hidden',
		'#value' => $input_id,
	);

	$query = db_select('boa_input', 'i');
	$query->fields('i', array('name', 'path', 'run_opts', 'compiler_path', 'enabled'));
	$query->condition('i.id', $input_id);

	$result = $query->execute();
	$record = $result->fetchObject();

	$f['name']['#default_value'] = $record->name;
	$f['path']['#default_value'] = $record->path;
	$f['compilerpath']['#default_value'] = $record->compiler_path;
	$f['opts']['#default_value'] = $record->run_opts;
	$f['enabled']['#default_value'] = $record->enabled;

	return $f;
}

function boa_dataset_edit_form_validate($form, &$form_state) {
	if (strlen($form_state['values']['path']) > 0) {
		$path = $form_state['values']['path'];
		$out = system(hadoop_exec_path() . ' fs -ls /repcache/' . escapeshellcmd($path) . '/projects.seq > /dev/null', $retval);
		if ($retval != 0)
			form_set_error('path', t('HDFS Folder Name ') . "'$path'" . t(' does not exist or is not a valid input dataset.'));
	}

	if (strlen($form_state['values']['compilerpath']) > 0) {
		$compiler_base = boa_variable_get('compiler_path') . '/';
		$compiler_path = $form_state['values']['compilerpath'];
		if (!is_dir($compiler_base . $compiler_path))
			form_set_error('compilerpath', t('Compiler Folder Name ') . "'$compiler_path'" . t(' does not exist.'));
		else if (!is_file($compiler_base . $compiler_path . '/dist/boa-runtime.jar'))
			form_set_error('compilerpath', t('Compiler Folder Name ') . "'$compiler_path'" . t(' does not contain a runtime JAR.'));
		else if (!is_file($compiler_base . $compiler_path . '/dist/boa-compiler.jar'))
			form_set_error('compilerpath', t('Compiler Folder Name ') . "'$compiler_path'" . t(' does not contain a compiler JAR.'));
	}
}

function boa_dataset_edit_form_submit($form, &$form_state) {
	$input_id = (int)$form_state['values']['id'];

	$query = db_update('boa_input');
	$query->fields(array(
		'name' => $form_state['values']['name'],
		'path' => strlen($form_state['values']['path']) > 0 ? $form_state['values']['path'] : NULL,
		'run_opts' => $form_state['values']['opts'],
		'compiler_path' => strlen($form_state['values']['compilerpath']) > 0 ? $form_state['values']['compilerpath'] : NULL,
		'enabled' => isset($form_state['values']['enabled']) ? (int)$form_state['values']['enabled'] : 0,
	));
	$query->condition('id', $input_id);
	$query->execute();

	drupal_set_message(t('Dataset was updated'), 'status');
	drupal_goto("boa/datasets/$input_id/edit");
	drupal_exit();
}

function boa_dataset_list() {
	global $user;

	$header = array(
		array(
			'data' => t('ID'),
			'field' => 'id',
			'sort' => 'desc',
			'descending' => false,
		),
		array(
			'data' => t('Name'),
			'field' => 'name',
			'descending' => true,
		),
		array(
			'data' => t('HDFS Path'),
			'field' => 'path',
			'descending' => true,
		),
		array(
			'data' => t('Compiler Path'),
			'field' => 'compiler_path',
			'descending' => true,
		),
		array(
			'data' => t('Run Options'),
			'field' => 'run_opts',
			'descending' => true,
		),
		array(
			'data' => t('Released'),
			'field' => 'enabled',
			'descending' => true,
		),
		array(
			'data' => t('Commands'),
		),
	);

	$query = db_select('boa_input', 'i');
	$query->fields('i', array('id', 'name', 'path', 'compiler_path', 'run_opts', 'enabled'));

	$user_data = field_get_items('user', user_load($user->uid), 'field_table_rows');
	$maxRows = empty($user_data) ? 10 : $user_data[0]['value'];
	$query = $query->extend('TableSort')->orderByHeader($header)->extend('PagerDefault')->limit($maxRows);

	$result = $query->execute();

	$rows = array();
	while ($data = $result->fetchObject())
		$rows[] = array(
			$data->id,
			$data->name,
			$data->path,
			$data->compiler_path,
			$data->run_opts,
			$data->enabled ? 'Yes' : 'NO',
			"[<a href=\"?q=boa/datasets/{$data->id}/edit\">" . t('edit') . '</a>]',
		);

	$options = array(
		'header' => $header,
		'rows' => $rows,
		'attributes' => array('width' => '100%'),
		'sticky' => true,
		'caption' => NULL,
		'colgroups' => array(),
		'caption' => '<a href="index.php?q=boa/datasets/add">' . t('Add a dataset') . '</a>',
		'empty' => t('No datasets exist yet.'),
	);

	return theme_table($options).theme('pager');
}

?>
