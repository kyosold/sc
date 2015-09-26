<?php

include_once('common.php');
include_once('utils.php');

$name = trim(avoid_sql($_POST['name']));
$uid = trim(avoid_sql($_POST['uid']));
$page = trim(avoid_sql($_POST['page']));

$page = 0;

$res = array();

if (strlen($name) > 0) {

	$members = array();
	
	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
		
		$db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
		$db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
		
		$sql = "SELECT id, nickname, birthday, gender, status FROM sc_user WHERE nickname like '%{$name}%' LIMIT ". ($page * ROWS_OF_PAGE) .", ". ROWS_OF_PAGE;
//echo $sql;
		$stmt = $db->prepare($sql);
		$stmt->execute();
		while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			
			if ($row['id'] == $uid) {
				// 不允许出现自己
				continue;
			}
			
			$data = array();
        	$data['id'] = $row['id'];
        	$data['nickname'] = $row['nickname'];
        	$data['birthday'] = $row['birthday'];
        	$data['gender'] = $row['gender'];
        	$data['status'] = $row['status'];
        	$data['icon'] = get_avatar_url($row['id']);
        
        	$members[] = $data;
			
		}
		
		$res = show_info('succ', '查询成功');
    	$res['data'] = $members;
    
		echo json_encode($res);

    	return 0;
		
		
	} catch (PDOException $e) {
		log_info("find user fail: ".$e->getMessage());
		
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

