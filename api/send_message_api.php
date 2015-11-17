<?php

include_once('common.php');
include_once('utils.php');

$uid = trim(avoid_sql($_POST['uid']));
$nickname = trim(avoid_sql($_POST['nickname']));
$tuid = trim(avoid_sql($_POST['tuid']));
$tag_type = trim(avoid_sql($_POST['type']));
$message = trim(avoid_sql($_POST['message'];

$res = array();

if (strlen($uid) > 0 && strlen($tuid) > 0) {

	 try {   
        
        $queue_type = "CHAT";	// system delete friend relationship

		$need_notice = true;
		if ($need_notice == true) {
			$pid = "0";
			$nickname = base64_encode($nickname); 

			// 发送一个通知
			$ret = send_notice_to_uid($uid, $nickname, $tuid, $tpid, $tag_type, $pid);
			if ($ret == true) {
				$res = show_info('succ', '处理成功');
			} else {
				$res = show_info('succ', '处理成功');
			}
			echo json_encode($res);

			log_info("send notice to uid: {$ret}");

			return 0;
			
		} else {
			log_info("send message to api: 请选择执行的动作");
			$res = show_info('fail', '请选择执行的动作');
			echo json_encode($res);
			return 0;
		}
	
	} catch (PDOException $e) {
		$db->rollback();
		
		log_info("send message to api fail, ".$e->getMessage());
		$res = show_info('fail', $e->getMessage());
        $res['localdes'] = $e->getMessage();
        echo json_encode($res);
		return 1;
	}		

    return 0;

} else {
	log_info("send message to api fail: parameters error");
	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}




echo json_encode($res);

?>

