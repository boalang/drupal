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

function _job_output_rpc($jobId) {
	if (!isValidJob($jobId))
		invalidJob($jobId);

	set_time_limit(0);

	$query = db_select('boa_jobs', 'r');
	$query->join('boa_output', 'o', 'r.id = o.id');
	$query->addExpression('COALESCE(o.hash, \'\')', 'hash');
	$query->addExpression('COALESCE(o.length, 0)', 'length');
	$query->condition('r.id', $jobId);

	$record = $query->execute()->fetchObject();

	if (strlen($record->hash) == 0 || $record->length < 1)
		return t('The job has no output.');

	$dstdir = $jobId . '/' . $record->hash;
	$filename = "boa-job$jobId-output.txt";

	$dir = '/home/boa/www/boa/output/' . $dstdir;
	$path = $dir . '/' . $filename;

	if (!file_exists($path) || $record->length != filesize($path)) {
		$tmp = tempnam("/tmp", "boa-output");
		$tmpcrc = str_replace('boa-output', '.boa-output', $tmp) . '.crc';
		@unlink($tmp);
		@unlink($tmpcrc);
		drupal_register_shutdown_function('download_tmp_cleanup', $tmp, $tmpcrc);

		set_time_limit(0);
		system("/home/hadoop/hadoop-current/bin/hadoop fs -getmerge /boa/$jobId/ $tmp");

		if (filesize($tmp) == $record->length) {
			@mkdir($dir, 0700, true);
			rename($tmp, $path);
		}

		@unlink($tmp);
		@unlink($tmpcrc);

		if (filesize($tmp) != $record->length)
			return t('Unable to fetch output.');
	}

	return "http://boa.cs.iastate.edu/boa/output/$dstdir/$filename";
}

function boa_job_download($jobId) {
	if (!isValidJob($jobId))
		invalidJob($jobId);
	_download_job($jobId, "boa/job/$jobId");
}

function _download_job($jobId, $fallbackUrl) {
	global $user;

	set_time_limit(0);

	drupal_set_title(t('Output for Job ') . $jobId);

	$query = db_select('boa_jobs', 'r');
	$query->join('boa_output', 'o', 'r.id = o.id');
	$query->fields('r', array('hadoop_status'));
	$query->addExpression('COALESCE(o.length, 0)', 'length');
	$query->condition('r.id', $jobId);

	$record = $query->execute()->fetchObject();

	if ($record->hadoop_status == 2) {
		if ($record->length > 0) {
			$tmp = tempnam("/tmp", "boa-output");
			$tmpcrc = str_replace('boa-output', '.boa-output', $tmp) . '.crc';
			@unlink($tmp);
			@unlink($tmpcrc);
			drupal_register_shutdown_function('download_tmp_cleanup', $tmp, $tmpcrc);

			set_time_limit(0);
			system("/home/hadoop/hadoop-current/bin/hadoop fs -getmerge /boa/$jobId/ $tmp");

			if (filesize($tmp) != $record->length) {
				@unlink($tmp);
				@unlink($tmpcrc);

				drupal_set_message(t('There was a problem getting the output. Try again later, or contact support.'), 'error');
				drupal_goto($fallbackUrl);
				drupal_exit();
			}

			$file = fopen($tmp, 'r');

			drupal_add_http_header('Content-Type', 'text; utf-8');
			drupal_add_http_header('Content-Length', $record->length);
			drupal_add_http_header('Content-Disposition', "attachment; filename=\"boa-job$jobId-output.txt\"");

			while (!feof($file)) {
				print fread($file, 65536);
				while (@ob_end_flush());
				@flush();
			}

			fclose($file);
			@unlink($tmp);
			@unlink($tmpcrc);

			drupal_exit();
		}

		drupal_set_message(t('The job finished successfully, but has no output.'), 'warning');
		drupal_goto($fallbackUrl);
		drupal_exit();
	}

	drupal_set_message(t('The job has not successfully finished running.'), 'warning');
	drupal_goto($fallbackUrl);
	drupal_exit();
}

function download_tmp_cleanup($tmp, $tmpcrc) {
	@unlink($tmp);
	@unlink($tmpcrc);
}

?>
