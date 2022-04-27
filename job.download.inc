<?php

function _job_output_size_rpc($jobId) {
	if (!isValidJob($jobId))
		invalidJob($jobId);

	$query = db_select('boa_jobs', 'r');
	$query->join('boa_output', 'o', 'r.id = o.id');
	$query->addExpression('COALESCE(o.length, 0)', 'length');
	$query->condition('r.id', $jobId);

	$result = $query->execute();
	if ($result->rowCount() == 0)
		return 0;

	$record = $result->fetchObject();
	return $record->length;
}

function _job_output_rpc($jobId, $public) {
	if (!$public && !isValidJob($jobId))
		invalidJob($jobId);

	set_time_limit(0);

	$query = db_select('boa_jobs', 'r');
	$query->join('boa_output', 'o', 'r.id = o.id');
	$query->fields('r', array('compiler_status', 'hadoop_status'));
	$query->addExpression('COALESCE(o.hash, \'\')', 'hash');
	$query->addExpression('COALESCE(o.length, 0)', 'length');
	$query->condition('r.id', $jobId);

	$record = $query->execute()->fetchObject();

	if ($record->compiler_status != 2 || $record->hadoop_status != 2)
		return t('The job is still running.');

	if (strlen($record->hash) == 0 || $record->length < 1)
		return t('The job finished successfully, but has no output.');

	$dstdir = $jobId . '/' . $record->hash;
	$filename = "boa-job$jobId-output.txt";

	$dir = boa_path_get('output_path') . '/' . $dstdir;
	$path = $dir . '/' . $filename;

	if (!file_exists($path) || $record->length != filesize($path)) {
		$tmp = tempnam("/tmp", "boa-output");
		$tmpcrc = str_replace('boa-output', '.boa-output', $tmp) . '.crc';
		@unlink($tmp);
		@unlink($tmpcrc);
		drupal_register_shutdown_function('download_tmp_cleanup', $tmp, $tmpcrc);

		set_time_limit(0);
		system(hadoop_exec_path() . " fs -getmerge /boa/$jobId/ $tmp");

		if (filesize($tmp) == $record->length) {
			@mkdir($dir, 0700, true);
			rename($tmp, $path);
			@unlink($tmp);
			@unlink($tmpcrc);
		} else {
			@unlink($tmp);
			@unlink($tmpcrc);
			return t('There was a problem getting the output. Try again later, or contact support.');
		}
	}

	$proto = "http";
	if (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] === 'on')
		$proto = "https";
	$outputpath = boa_variable_get('output_path_url');
	return "$proto://$_SERVER[HTTP_HOST]/$outputpath/$dstdir/$filename";
}

function boa_job_download($jobId) {
	_download_job($jobId, "boa/job/$jobId", false);
}

function _download_job($jobId, $fallbackUrl, $public) {
	$res = _job_output_rpc($jobId, $public);

	drupal_set_title(t('Output for Job ') . $jobId);

	if (strpos($res, "http") == 0) {
		drupal_goto($res);
		drupal_exit();
	}

	drupal_set_message($res, 'warning');
	drupal_goto($fallbackUrl);
	drupal_exit();
}

function download_tmp_cleanup($tmp, $tmpcrc) {
	@unlink($tmp);
	@unlink($tmpcrc);
}

?>
