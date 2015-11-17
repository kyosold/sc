<?php

include_once('common.php');
include_once('utils.php');

$uid = trim(avoid_sql($_POST['uid']));
$qid = trim(avoid_sql($_POST['qid']));

$res = array();

if (strlen($uid) > 0 && strlen($qid) > 0) {

	 try {   
        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
            
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		// 更新已读qid最大数
		$sql = "UPDATE sc_user SET read_qid = {$qid} WHERE id = {$uid} LIMIT 1";
		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}
			
		$res = show_info('succ', '处理成功');

		echo json_encode($res);

		log_info("set read maxqid action succ");

		return 0;
			
	
	} catch (PDOException $e) {
		
		log_info("set private friend action fail, ".$e->getMessage());
		$res = show_info('fail', $e->getMessage());
        $res['localdes'] = $e->getMessage();
        echo json_encode($res);
		return 1;
	}		

    return 0;

} else {
	log_info("set read maxqid action fail");
	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}




echo json_encode($res);

?>

