<?php

include_once('common.php');
include_once('utils.php');

$qid = 0;

$uid = trim(avoid_sql($_POST['uid']));
$qid = trim(avoid_sql($_POST['qid']));

$res = array();

if ((strlen($uid) > 0) && (strlen($qid) > 0)) {

	$msg_queue = array();

	try {
		$sql = "";
		$select = "SELECT id, mid, tag_type, fuid, fnick, tuid, queue_type, queue_file, queue_size, image_wh, cdate, expire, fdel, tdel FROM sc_queue ";
		$where = " fuid = {$uid} AND id > {$qid} ";
		$sql = $select." WHERE ".$where;

		$stmt = $db->prepare($sql);
        $stmt->execute();
        while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {

            $item = array();
            $item['id'] = $row['id'];
            $item['mid'] = $row['mid'];
            $item['tag_type'] = $row['tag_type'];
            $item['fuid'] = $row['fuid'];
            $item['fnick'] = $row['fnick'];
            $item['tuid'] = $row['tuid'];
            $item['queue_type'] = $row['queue_type'];
            $item['cdate'] = $row['cdate'];
            $item['expire'] = $row['expire'];
            $item['fdel'] = $row['fdel'];
            $item['tdel'] = $row['tdel'];

            $queue[] = $item;

        }
		$res['queue'] = $queue;

		echo json_encode($res);

        return 0;

	} catch (PDOException $e) {
		log_info("get unread message fail: ".$e->getMessage());

		$res = show_info('fail', $e->getMessage());
		echo json_encode($res);
		return 1;
	}

} else {
	$res['status'] = 'fail';
    $res['des'] = 'parameters error';
}

echo json_encode($res);


?>

